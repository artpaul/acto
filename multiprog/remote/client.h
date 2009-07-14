
#ifndef acto_remote_client_h_495808c30ca7465eb25ec855541ff6e8
#define acto_remote_client_h_495808c30ca7465eb25ec855541ff6e8

#include <map>

#include <system/mutex.h>
#include <system/event.h>
#include <remote/libsocket/libsocket.h>

#include <core/types.h>

#include "protocol.h"
#include "transport.h"

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
    /// Регистрационная информация об объекте
    /// который выступает в роли сервера
    struct actor_info_t {
        actor_t     actor;
        ui64        id;
    };

    typedef std::map<ui64, actor_info_t>            actors_t;

    typedef std::map< std::string, actor_info_t >   global_t;

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
    /// -
    void         enable_server();
    /// -
    void         register_actor(const actor_t& actor, const char* path);

private:
    static void do_client_commands(const ui16 cmd, stream_t* s, void* param);
    static void do_server_commands(const ui16 cmd, stream_t* s, void* param);

private:
    core::mutex_t   m_cs;
    actors_t        m_actors;
    global_t        m_registered;
    senders_t       m_senders;
    core::event_t   m_event_getid;
    volatile ui64   m_last;
    /// Генератор идентификаторов для объектов
    ui64            m_counter;

    transport_t     m_transport;

};

} // namespace remote

} // namespace acto

#endif // acto_remote_client_h_495808c30ca7465eb25ec855541ff6e8
