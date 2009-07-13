
#ifndef acto_remote_client_h_495808c30ca7465eb25ec855541ff6e8
#define acto_remote_client_h_495808c30ca7465eb25ec855541ff6e8

#include <map>

#include <system/mutex.h>
#include <system/event.h>
#include <remote/libsocket/libsocket.h>

#include <core/types.h>

#include "protocol.h"


namespace acto {

class actor_t;

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

    typedef std::map< ui64, core::object_t* >       senders_t;

public:
    remote_module_t();

    ~remote_module_t();

    static remote_module_t* instance();

public:
    /// -
    virtual void handle_message(core::package_t* const package);
    /// Отправить сообщение соответствующему объекту
    virtual void send_message(core::package_t* const package);

    virtual void shutdown(core::event_t& event);

    virtual void startup();

    actor_t      connect(const char* path, unsigned int port);

private:
    static void read_actor_connect(int s, SOEVENT* const ev);

    int get_host_connection(const char* host, unsigned int port);

private:
    core::mutex_t   m_cs;
    host_map_t      m_hosts;
    actors_t        m_actors;
    senders_t       m_senders;
    core::event_t   m_event_getid;
    volatile ui64   m_last;

};

} // namespace remote

} // namespace acto

#endif // acto_remote_client_h_495808c30ca7465eb25ec855541ff6e8
