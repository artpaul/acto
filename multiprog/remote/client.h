
#ifndef acto_remote_client_h_495808c30ca7465eb25ec855541ff6e8
#define acto_remote_client_h_495808c30ca7465eb25ec855541ff6e8

#include <map>

#include <system/mutex.h>
#include <remote/libsocket/libsocket.h>

#include "protocol.h"


namespace acto {

namespace remote {

/**
 */
class client_t {
    struct remote_host_t {
        std::string name;
        int         fd;
        int         port;
    };

    struct actor_info_t {
        uint64_t    id;
        actor_t     ref;
    };

    typedef std::map<std::string, remote_host_t>    host_map_t;
    typedef std::map<uint64_t, actor_info_t>        actors_t;

    core::mutex_t   m_cs;
    host_map_t      m_hosts;
    actors_t        m_actors;

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
                uint16_t    cmd = 0;
                int64_t     id  = 0;

                so_readsync(s, &cmd, sizeof(cmd), 5);
                so_readsync(s, &id,  sizeof(id),  5);

                if (cmd == ACTOR_REFERENCE && id > 0) {
                    actor_info_t    info;

                    info.id  = id;
                    info.ref = actor_t();

                    pthis->m_actors[id] = info;
                    printf("actor id: %Zu\n", id);
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
    }

    ~client_t() {
        so_terminate();
    }

    static client_t* instance() {
        static client_t value;

        return &value;
    }

public:
    actor_t connect(const char* path, unsigned int port) {
        int fd = get_host_connection("127.0.0.1", port);

        if (fd > 0) {
            printf("so_connect\n");
            so_pending(fd, SOEVENT_READ, 100, &read_actor_connect, this);
            // !!! Получить данные об актёре
            {
                uint16_t    id  = ACTOR_REFERENCE;
                uint32_t    len = strlen("server");

                so_sendsync(fd, &id,  sizeof(id));
                so_sendsync(fd, &len, sizeof(len));
                so_sendsync(fd, "server", len);
            }
        }
        return actor_t();
    }
};

} // namespace remote

} // namespace acto

#endif // acto_remote_client_h_495808c30ca7465eb25ec855541ff6e8
