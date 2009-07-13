
#include <generic/memory.h>
#include <act_user.h>

#include "client.h"

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
remote_module_t::remote_module_t() {
    so_init();
    core::runtime_t::instance()->register_module(this, 1);
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
    int fd = get_host_connection("127.0.0.1", port);

    if (fd > 0) {
        m_event_getid.reset();
        // -
        so_pending(fd, SOEVENT_READ, 100, &read_actor_connect, this);
        // !!! Получить данные об актёре
        {
            ui16    id  = ACTOR_REFERENCE;
            ui32    len = strlen("server");

            so_sendsync(fd, &id,  sizeof(id));
            so_sendsync(fd, &len, sizeof(len));
            so_sendsync(fd, "server", len);
        }
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
void remote_module_t::read_actor_connect(int s, SOEVENT* const ev) {
    remote_module_t* const pthis = static_cast<remote_module_t*>(ev->param);

    switch (ev->type) {
    case SOEVENT_CLOSED:
        // Соединение с сервером было закрыто
        printf("closed\n");
        return;
    case SOEVENT_READ:
        {
            // code, len|name, id,
            ui16    cmd = 0;
            ui64    id  = 0;

            so_readsync(s, &cmd, sizeof(cmd), 5);
            so_readsync(s, &id,  sizeof(id),  5);

            if (cmd == ACTOR_REFERENCE && id > 0) {
                actor_info_t    info;

                info.id  = id;
                info.ref = actor_t();

                pthis->m_last       = id;
                pthis->m_actors[id] = info;
                pthis->m_event_getid.signaled();
            }
        }
        break;
    }
}
//-----------------------------------------------------------------------------
int remote_module_t::get_host_connection(const char* host, unsigned int port) {
    const std::string    host_str = std::string(host);
    host_map_t::iterator i        = m_hosts.find(host_str);

    if (i != m_hosts.end()) {
        return (*i).second.fd;
    }
    else {
        // Установить подключение
        int fd = so_socket(SOCK_STREAM);

        if (fd != -1) {
            if (so_connect(fd, inet_addr("127.0.0.1"), port) != 0)
                so_close(fd);
            else {
                remote_host_t data;

                data.name = host_str;
                data.fd   = fd;
                data.port = port;
                m_hosts[host_str] = data;

                return fd;
            }
        }
    }
    return 0;
}

} // namespace remote

} // namespace acto
