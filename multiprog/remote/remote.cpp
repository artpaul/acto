
#include <acto.h>

#include <map>

#include <generic/memory.h>
#include <system/mutex.h>
#include <remote/libsocket/libsocket.h>

#include "remote.h"
#include "client.h"
#include "protocol.h"

#define CLIENTPORT    25121

namespace acto {

namespace remote {

/**
 */
class server_t {
    struct actor_info_t {
        actor_t     actor;
        ui64        id;
    };

    typedef std::map<std::string, actor_info_t>  global_t;

    core::mutex_t   m_cs;
    global_t        m_actors;
    int             m_fd;
    ui64            m_counter;

private:
    static void read_data(int s, SOEVENT* const ev) {
        server_t* const pthis = static_cast<server_t*>(ev->param);

        switch (ev->type) {
        case SOEVENT_CLOSED:
            printf("closed\n");
            return;
        case SOEVENT_TIMEOUT:
            printf("time out\n");
            break;
        case SOEVENT_READ:
            {
                uint16_t    id = 0;

                so_readsync(s, &id, sizeof(id), 5);

                switch (id) {
                    case ACTOR_REFERENCE:
                        {
                            printf("ACTOR_REFERENCE\n");

                            ui32        len;
                            std::string name;

                            so_readsync(s, &len, sizeof(len), 5);
                            {
                                generics::array_ptr<char>   buf(new char[len + 1]);

                                so_readsync(s, buf.get(), len, 5);
                                buf[len] = '\0';
                                name = buf.get();
                            }

                            {
                                ui16    cmd = ACTOR_REFERENCE;
                                ui64    aid = 0;

                                global_t::iterator i = pthis->m_actors.find(name);
                                if (i != pthis->m_actors.end()) {
                                    printf("has: %s\n", name.c_str());
                                    aid = (*i).second.id;
                                }

                                so_sendsync(s, &cmd, sizeof(cmd));
                                so_sendsync(s, &aid, sizeof(aid));
                            }
                        }
                        break;
                    case SEND_MESSAGE:
                        {
                            printf("SEND_MESSAGE\n");

                            ui64        len;
                            so_readsync(s, &len, sizeof(len), 5);
                            {
                                ui64    oid;
                                ui64    tid;
                                size_t  size = len - 2 * sizeof(ui64);

                                so_readsync(s, &oid, sizeof(oid), 5);
                                so_readsync(s, &tid, sizeof(tid), 5);

                                generics::array_ptr< char >   buf(new char[size + 1]);

                                so_readsync(s, buf.get(), size, 5);
                                buf[len] = '\0';
                                //name = buf.get();

                                // 1. По коду сообщения получить класс сообщения
                                msg_metaclass_t* meta = core::message_map_t::instance()->find_metaclass(tid);
                                if (meta != NULL && meta->make_instance != NULL) {
                                    msg_t* const msg = meta->make_instance();
                                    meta->serializer->read(msg, buf.get(), size);

                                    for (global_t::iterator i = pthis->m_actors.begin(); i != pthis->m_actors.end(); ++i) {
                                        if ((*i).second.id == oid) {
                                            printf("finded\n");
                                            core::runtime_t::instance()->send(0, (*i).second.actor.data(), msg);
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
            break;
        }
        so_pending(s, SOEVENT_READ, 10, &read_data, ev->param);
    }

    static void do_host_connected(int s, SOEVENT* const ev) {
        switch (ev->type) {
        case SOEVENT_ACCEPTED:
            printf("accepted\n");
            so_pending(((SORESULT_LISTEN*)ev->data)->client, SOEVENT_READ, 10, &read_data, ev->param);
            break;
        case SOEVENT_TIMEOUT:
            printf("accept timeout\n");
            break;
        }
    }

public:
    server_t()
        : m_fd(0)
        , m_counter(100)
    {
        m_fd = so_socket(SOCK_STREAM);

        if (m_fd != -1) {
            so_listen(m_fd, inet_addr("127.0.0.1"), CLIENTPORT,  5, &do_host_connected, this);
        }
        else {
            printf("create error\n");
            m_fd = 0;
        }
    }

    static server_t* instance() {
        static server_t value;

        return &value;
    }

public:
    void register_actor(const actor_t& actor, const char* path) {
        if (actor.assigned()) {
            core::MutexLocker lock(m_cs);
            actor_info_t    info;

            info.actor = actor;
            info.id    = ++m_counter;

            m_actors[std::string(path)] = info;
        }
    }
};


///////////////////////////////////////////////////////////////////////////////
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

// Подключиться к серверу
// Вызывается на стороне клиента
actor_t connect(const char* path, unsigned int port) {
    return remote_module_t::instance()->connect(path, port);
}

// Зарегистрировать актера в словаре.
// Вызывается на стороне сервера
void register_actor(const actor_t& actor, const char* path) {
    server_t::instance()->register_actor(actor, path);
}

} // namespace remote

} // namespace acto
