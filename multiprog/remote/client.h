
#ifndef acto_remote_client_h_495808c30ca7465eb25ec855541ff6e8
#define acto_remote_client_h_495808c30ca7465eb25ec855541ff6e8

#include <map>

#include <system/mutex.h>
#include <system/event.h>
#include <remote/libsocket/libsocket.h>

#include "protocol.h"


namespace acto {

namespace remote {

/**
 */
class remote_base_t : public core::actor_body_t {
public:
    ui64    m_id;
    /// Сокет взаимодействия с удаленным хостом
    int     m_fd;
};

/**
 */
class remote_module_t : public core::module_t {
    struct remote_host_t {
        std::string name;
        int         fd;
        int         port;
    };

    struct actor_info_t {
        ui64        id;
        actor_t     ref;
    };

    typedef std::map<std::string, remote_host_t>    host_map_t;
    typedef std::map<ui64, actor_info_t>            actors_t;

    core::mutex_t   m_cs;
    host_map_t      m_hosts;
    actors_t        m_actors;
    core::event_t   m_event_getid;
    volatile ui64   m_last;

private:
    static void read_actor_connect(int s, SOEVENT* const ev) {
        remote_module_t* const pthis = static_cast<remote_module_t*>(ev->param);

        switch (ev->type) {
        case SOEVENT_CLOSED:
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

                    pthis->m_actors[id] = info;
                    printf("actor id: %Zu\n", id);
                    pthis->m_last = id;
                    pthis->m_event_getid.signaled();
                }
            }
            break;
        }
    }

    int get_host_connection(const char* host, unsigned int port) {
        host_map_t::iterator i = m_hosts.find(std::string(host));

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

                    data.name = std::string(host);
                    data.fd   = fd;
                    data.port = port;
                    m_hosts[std::string(host)] = data;

                    return fd;
                }
            }
        }
        return 0;
    }

public:
    remote_module_t() {
        so_init();
        core::runtime_t::instance()->register_module(this, 1);
    }

    ~remote_module_t() {
        so_terminate();
    }

    static remote_module_t* instance() {
        static remote_module_t value;

        return &value;
    }

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

public:
    /// -
    virtual void handle_message(core::package_t* const package) {
        // -
    }
    /// Отправить сообщение соответствующему объекту
    virtual void send_message(core::package_t* const package) {
        core::object_t* const target = package->target;
        remote_base_t*  const impl   = static_cast< remote_base_t* >(target->impl);
        const msg_t*    const msg    = package->data;
        // -
        assert(target->module == 1);

        // 1. преобразовать текущее сообщение в байтовый поток
        acto::serializer_t* const s = msg->meta->serializer.get();
        // -
        mem_stream_t    mem;

        s->write(msg, &mem);

        // 2. передать по сети
        // добавить заголовок к данным сообщения
        //  id-объекта, id-сообщения, длинна данных
        ui64    len = (2 * sizeof(ui64)) + mem.m_size;
        ui16    cmd = SEND_MESSAGE;
        ui64    tid = msg->meta->tid;

        printf("%i\n", (int)mem.m_size);

        so_sendsync(impl->m_fd, &cmd, sizeof(cmd));
        so_sendsync(impl->m_fd, &len, sizeof(len));
        so_sendsync(impl->m_fd, &impl->m_id, sizeof(impl->m_id));
        so_sendsync(impl->m_fd, &tid, sizeof(tid));
        so_sendsync(impl->m_fd, mem.m_buf.get(), mem.m_size);

        printf("sending remote message\n");
        delete package;
    }

    virtual void shutdown(core::event_t& event) {
        event.signaled();
    }

    virtual void startup() {
    }

    actor_t connect(const char* path, unsigned int port) {
        int fd = get_host_connection("127.0.0.1", port);

        if (fd > 0) {
            m_event_getid.reset();
            printf("so_connect\n");
            so_pending(fd, SOEVENT_READ, 100, &read_actor_connect, this);
            // !!! Получить данные об актёре
            {
                ui16    id  = ACTOR_REFERENCE;
                ui32    len = strlen("server");

                so_sendsync(fd, &id,  sizeof(id));
                so_sendsync(fd, &len, sizeof(len));
                so_sendsync(fd, "server", len);
            }
            m_event_getid.wait();
            printf("after wait\n");
            //
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
};

} // namespace remote

} // namespace acto

#endif // acto_remote_client_h_495808c30ca7465eb25ec855541ff6e8
