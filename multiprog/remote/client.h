
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
class client_t : public core::module_t {
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
        client_t* const pthis = static_cast<client_t*>(ev->param);

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
    client_t() {
        so_init();
        core::runtime_t::instance()->register_module(this, 1);
    }

    ~client_t() {
        so_terminate();
    }

    static client_t* instance() {
        static client_t value;

        return &value;
    }

public:
    /// -
    virtual void handle_message(core::package_t* const package) {
        // -
    }
    /// Отправить сообщение соответствующему объекту
    virtual void send_message(core::package_t* const package) {
        core::object_t* const target = package->target;
        remote_base_t*  const impl   = static_cast<remote_base_t*>(target->impl);
        // -
        assert(target->module == 1);

        // 1. преобразовать текущее сообщение в байтовый поток
        // 2. передать по сети

        //package->data;
        //package->type;

        printf("sending remote message\n");
        delete package;
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
