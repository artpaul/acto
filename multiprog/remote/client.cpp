
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

    size_t size() const {
        return m_size;
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
    core::runtime_t::instance()->register_module(this, 1);

    m_transport.client_handler(&remote_module_t::do_client_commands, this);
    m_transport.server_handler(&remote_module_t::do_server_commands, this);
}
//-----------------------------------------------------------------------------
remote_module_t::~remote_module_t() {
    // -
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
        for (actors_t::iterator i = m_actors.begin(); i != m_actors.end(); ++i) {
            if ((*i).second.data() == sender) {
                sid = (*i).first;
                break;
            }
        }
        if (sid == 0) {
            sid = ++m_last;
            m_actors[sid] = actor_t(sender);
        }
    }

    // 2. передать по сети
    // добавить заголовок к данным сообщения
    //  id-объекта, id-сообщения, длинна данных
    ui64    len = (3 * sizeof(ui64)) + mem.m_size;
    ui64    tid = msg->meta->tid;
    ui16    cmd = SEND_MESSAGE;

    transport_msg_t ch;

    ch.write(&cmd, sizeof(cmd));
    ch.write(&len, sizeof(len));
    ch.write(&sid, sizeof(sid));
    ch.write(&impl->m_id, sizeof(impl->m_id));
    ch.write(&tid, sizeof(tid));
    // - запись данных напрямую в поток
    s->write(package->data, &ch);
    m_transport.send_message(impl->m_node, ch);

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
    network_node_t* const node = m_transport.connect("127.0.0.1", port);

    if (node != NULL) {
        ask_data_t* const ask = new ask_data_t();
        ui64    aid = ++m_last;

        ask->oid = 0;
        ask->event.reset();
        m_asks[aid] = ask;

        // -
        // !!! Получить данные об актёре

        ui32            len = strlen("server");
        ui16            cmd = ACTOR_REFERENCE;
        transport_msg_t ch;
        // -
        ch.write(&cmd, sizeof(cmd));
        ch.write(&aid, sizeof(aid));
        ch.write(&len, sizeof(len));
        ch.write("server", len);
        m_transport.send_message(node, ch);

        ask->event.wait(); // timed-wait
        // -
        if (ask->oid != 0) {
            remote_base_t* const value = new remote_base_t();
            // 1.
            value->m_id   = ask->oid;
            value->m_node = node;
            // 2. Создать объект ядра (счетчик ссылок увеличивается автоматически)
            core::object_t* const result = core::runtime_t::instance()->create_actor(value, 0, 1);

            m_actors[value->m_id] = actor_t(result);
            // -
            return actor_t(result);
        }

        m_asks.erase(aid);
        delete ask;
    }
    return actor_t();
}
//-----------------------------------------------------------------------------
void remote_module_t::enable_server() {
    m_transport.open_node(CLIENTPORT);
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
void remote_module_t::do_send_message(command_event_t* const ev, bool is_client) {
    ui64        len;

    ev->stream->read(&len, sizeof(len));
    {
        ui64    sid;
        ui64    oid;
        ui64    tid;
        size_t  size = len - 3 * sizeof(ui64);

        ev->stream->read(&sid, sizeof(sid));
        ev->stream->read(&oid, sizeof(oid));
        ev->stream->read(&tid, sizeof(tid));

        generics::array_ptr< char >   buf(new char[size + 1]);

        if (size > 0)
            ev->stream->read(buf.get(), size);
        // -
        buf[size] = '\0';

        // 1. По коду сообщения получить класс сообщения
        msg_metaclass_t* meta = core::message_map_t::instance()->find_metaclass(tid);
        if (meta != NULL && meta->make_instance != NULL) {
            msg_t* const msg = meta->make_instance();
            if (size > 0)
                meta->serializer->read(msg, buf.get(), size);

            if (is_client) {
                actors_t::iterator i = m_actors.find(oid);

                if (i != m_actors.end())
                    core::runtime_t::instance()->send(0, (*i).second.data(), msg);
            }
            else {
                for (global_t::iterator i = m_registered.begin(); i != m_registered.end(); ++i) {
                    if ((*i).second.id == oid) {
                        // Сформировать заглушку для удалённого объекта
                        actor_t sender;
                        {
                            remote_base_t* const value = new remote_base_t();
                            // 1.
                            value->m_id   = sid;
                            value->m_node = ev->node;
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
        }
    }
}

//-----------------------------------------------------------------------------
void remote_module_t::do_client_commands(command_event_t* const ev) {
    remote_module_t* const pthis = static_cast< remote_module_t* >(ev->param);

    switch (ev->cmd) {
        // Получение идентификатора объекта, для которого
        // был отправлен именнованный запрос
        case ACTOR_REFERENCE:
            {
                ui64 oid = 0;
                ui64 aid = 0;

                ev->stream->read(&aid, sizeof(aid));
                ev->stream->read(&oid, sizeof(oid));

                if (oid > 0 && aid > 0) {
                    core::MutexLocker   lock(pthis->m_cs);
                    ask_map_t::iterator i = pthis->m_asks.find(aid);

                    if (i != pthis->m_asks.end()) {
                        (*i).second->oid = oid;
                        (*i).second->event.signaled();
                    }
                }
            }
            break;
        case SEND_MESSAGE:
            pthis->do_send_message(ev, true);
            break;
    }
}
//-----------------------------------------------------------------------------
void remote_module_t::do_server_commands(command_event_t* const ev) {
    remote_module_t* const pthis = static_cast< remote_module_t* >(ev->param);

    switch (ev->cmd) {
        case ACTOR_REFERENCE:
            {
                ui32        len;
                ui64        aid;
                std::string name;

                /*
                    stream_t* s = m_transport.get_package_stream(ev);

                    s->read();
                */

                // получить запрос о ссылке на объект
                ev->stream->read(&aid, sizeof(aid));
                ev->stream->read(&len, sizeof(len));
                {
                    generics::array_ptr< char > buf(new char[len + 1]);

                    ev->stream->read(buf.get(), len);
                    buf[len] = '\0';
                    name = buf.get();
                }

                {
                    ui64 oid = 0;
                    ui16 cmd = ACTOR_REFERENCE;

                    global_t::iterator i = pthis->m_registered.find(name);
                    if (i != pthis->m_registered.end())
                        oid = (*i).second.id;

                    transport_msg_t ch;

                    ch.write(&cmd, sizeof(cmd));
                    ch.write(&aid, sizeof(aid));
                    ch.write(&oid, sizeof(oid));
                    pthis->m_transport.send_message(ev->node, ch);
                }
            }
            break;
        case SEND_MESSAGE:
            pthis->do_send_message(ev, false);
            break;
    }
}


} // namespace remote

} // namespace acto
