
#include <generic/memory.h>
#include <act_user.h>

#include "client.h"

#define CLIENTPORT    25121

namespace acto {

namespace remote {

/** */
class mem_stream_t : public stream_t {
public:
    size_t                      m_size;
    generics::array_ptr< char > m_buf;

public:
    mem_stream_t()
        : m_size(0)
        , m_buf(new char[1024])
    {
    }

    virtual void write(const void* buf, size_t size) {
        char* ptr = m_buf.get() + m_size;
        memcpy(ptr, buf, size);
        m_size += size;
    }
};


//-----------------------------------------------------------------------------
remote_module_t::remote_module_t()
    : m_last(0)
    , m_counter(0)
{
    so_init();
    core::runtime_t::instance()->register_module(this, 1);

    m_transport.client_handler(&remote_module_t::do_client_commands, this);
    m_transport.server_handler(&remote_module_t::do_server_commands, this);
}
//-----------------------------------------------------------------------------
remote_module_t::~remote_module_t() {
    so_terminate();
}
//-----------------------------------------------------------------------------
remote_module_t* remote_module_t::instance() {
    static remote_module_t value;

    return &value;
}

//-----------------------------------------------------------------------------
void remote_module_t::handle_message(core::package_t* const package) {
    // -
}
//-----------------------------------------------------------------------------
void remote_module_t::send_message(core::package_t* const package) {
    printf("send_message\n");

    core::object_t* const sender = package->sender;
    core::object_t* const target = package->target;
    remote_base_t*  const impl   = static_cast< remote_base_t* >(target->impl);
    const msg_t*    const msg    = package->data;
    // -
    assert(target->module == 1);

    // 1. преобразовать текущее сообщение в байтовый поток
    serializer_t* const s = msg->meta->serializer.get();
    // -
    mem_stream_t    mem;

    s->write(msg, &mem);

    ui64    sid = 0;

    {
        for (senders_t::iterator i = m_senders.begin(); i != m_senders.end(); ++i) {
            if ((*i).second == sender) {
                sid = (*i).first;
                break;
            }
        }
        if (sid == 0) {
            sid = ++m_last;
            m_senders[sid] = sender;
        }
    }

    // 2. передать по сети
    // добавить заголовок к данным сообщения
    //  id-объекта, id-сообщения, длинна данных
    ui64    len = (3 * sizeof(ui64)) + mem.m_size;
    ui16    cmd = SEND_MESSAGE;
    ui64    tid = msg->meta->tid;

    so_sendsync(impl->m_fd, &cmd, sizeof(cmd));
    so_sendsync(impl->m_fd, &len, sizeof(len));
    so_sendsync(impl->m_fd, &sid, sizeof(sid));
    so_sendsync(impl->m_fd, &impl->m_id, sizeof(impl->m_id));
    so_sendsync(impl->m_fd, &tid, sizeof(tid));
    so_sendsync(impl->m_fd, mem.m_buf.get(), mem.m_size);

    delete package;
}
//-----------------------------------------------------------------------------
void remote_module_t::shutdown(core::event_t& event) {
    event.signaled();
}
//-----------------------------------------------------------------------------
void remote_module_t::startup() {
    // -
}
//-----------------------------------------------------------------------------
actor_t remote_module_t::connect(const char* path, unsigned int port) {
    const int fd = m_transport.connect("127.0.0.1", port);

    if (fd > 0) {
        m_event_getid.reset();
        // -
        // !!! Получить данные об актёре

        /*
        data = m_transport.prepare_command(fd, ACTOR_REFERENCE);
        data.write(&len, sizeof(len));
        data.write("server", len);
        data.commit();
        */

        ui32      len = strlen("server");
        channel_t ch  = m_transport.send_command(fd, ACTOR_REFERENCE);
        // -
        ch.write(&len, sizeof(len));
        ch.write("server", len);

        m_event_getid.wait(); // timed-wait
        // -
        {
            remote_base_t* const value  = new remote_base_t();
            // 1.
            value->m_id = m_last;
            value->m_fd = fd;
            // 2. Создать объект ядра (счетчик ссылок увеличивается автоматически)
            core::object_t* const result = core::runtime_t::instance()->create_actor(value, 0, 1);

            return actor_t(result);
        }
    }
    return actor_t();
}
//-----------------------------------------------------------------------------
void remote_module_t::enable_server() {
    m_transport.open_server(CLIENTPORT);
}
//-----------------------------------------------------------------------------
void remote_module_t::register_actor(const actor_t& actor, const char* path) {
    if (actor.assigned()) {
        core::MutexLocker lock(m_cs);
        actor_info_t      info;

        info.actor = actor;
        info.id    = ++m_counter;

        m_registered[std::string(path)] = info;
    }
}
//-----------------------------------------------------------------------------
void remote_module_t::do_client_commands(const ui16 cmd, stream_t* s, void* param) {
    remote_module_t* const pthis = static_cast< remote_module_t* >(param);

    switch (cmd) {
        // Получение идентификатора объекта, для которого
        // был отправлен именнованный запрос
        case ACTOR_REFERENCE:
            {
                ui64 id = 0;

                s->read(&id, sizeof(id));

                if (id > 0) {
                    actor_info_t    info;

                    info.id    = id;
                    info.actor = actor_t();

                    pthis->m_last       = id;
                    pthis->m_actors[id] = info;
                    pthis->m_event_getid.signaled();
                }
            }
            break;
        case SEND_MESSAGE:
            break;
    }
}
//-----------------------------------------------------------------------------
void remote_module_t::do_server_commands(const ui16 cmd, stream_t* s, void* param) {
    remote_module_t* const pthis = static_cast< remote_module_t* >(param);

    switch (cmd) {
        case ACTOR_REFERENCE:
            {
                printf("ACTOR_REFERENCE\n");

                ui32        len;
                std::string name;
                // получить запрос о ссылке на объект
                s->read(&len, sizeof(len));
                {
                    generics::array_ptr< char > buf(new char[len + 1]);

                    s->read(buf.get(), len);
                    buf[len] = '\0';
                    name = buf.get();
                }

                {
                    ui16    cmd = ACTOR_REFERENCE;
                    ui64    aid = 0;

                    global_t::iterator i = pthis->m_registered.find(name);
                    if (i != pthis->m_registered.end()) {
                        printf("has: %s\n", name.c_str());
                        aid = (*i).second.id;
                    }

                    s->write(&cmd, sizeof(cmd));
                    s->write(&aid, sizeof(aid));
                }
            }
            break;
        case SEND_MESSAGE:
            {
                printf("SEND_MESSAGE\n");

                ui64        len;
                s->read(&len, sizeof(len));
                {
                    ui64    sid;
                    ui64    oid;
                    ui64    tid;
                    size_t  size = len - 3 * sizeof(ui64);

                    s->read(&sid, sizeof(sid));
                    s->read(&oid, sizeof(oid));
                    s->read(&tid, sizeof(tid));

                    generics::array_ptr< char >   buf(new char[size + 1]);

                    s->read(buf.get(), size);
                    buf[len] = '\0';

                    // 1. По коду сообщения получить класс сообщения
                    msg_metaclass_t* meta = core::message_map_t::instance()->find_metaclass(tid);
                    if (meta != NULL && meta->make_instance != NULL) {
                        msg_t* const msg = meta->make_instance();
                        meta->serializer->read(msg, buf.get(), size);

                        for (global_t::iterator i = pthis->m_registered.begin(); i != pthis->m_registered.end(); ++i) {
                            if ((*i).second.id == oid) {
                                // Сформировать заглушку для удалённого объекта
                                actor_t sender;
                                {
                                    remote_base_t* const value = new remote_base_t();
                                    // 1.
                                    value->m_id = ++pthis->m_last;
                                    value->m_fd = 0;//s;
                                    // 2. Создать объект ядра (счетчик ссылок увеличивается автоматически)
                                    core::object_t* const result = core::runtime_t::instance()->create_actor(value, 0, 1);

                                    sender = actor_t(result);
                                }
                                // -
                                printf("finded\n");
                                core::runtime_t::instance()->send(sender.data(), (*i).second.actor.data(), msg);
                                break;
                            }
                        }
                    }
                    // 2. Получить экземпляр объекта
                    // 3. Создать экземпляр сообщения
                }
            }
            break;
    }
}


} // namespace remote

} // namespace acto
