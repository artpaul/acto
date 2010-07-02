///////////////////////////////////////////////////////////////////////////////
//                           The act-o Library                               //
//---------------------------------------------------------------------------//
// Copyright © 2007 - 2010                                                   //
//     Pavel A. Artemkin (acto.stan@gmail.com)                               //
// ------------------------------------------------------------------ -------//
// License:                                                                  //
//     Code covered by the MIT License.                                      //
//     The authors make no representations about the suitability of this     //
//     software for any purpose. It is provided "as is" without express or   //
//     implied warranty.                                                     //
///////////////////////////////////////////////////////////////////////////////

#ifndef acto_remote_client_h_495808c30ca7465eb25ec855541ff6e8
#define acto_remote_client_h_495808c30ca7465eb25ec855541ff6e8

#include <map>

#include <system/mutex.h>
#include <system/event.h>

#include <core/types.h>

#include "protocol.h"
#include "transport.h"
#include "remote.h"

namespace acto {

class actor_t;

namespace remote {

/**
 */
class remote_base_t : public core::actor_body_t {
public:
    ui64            m_id;
    /// Сокет взаимодействия с удаленным хостом
    network_node_t* m_node;
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

    struct ask_data_t {
        core::event_t   event;
        ui64            oid;
    };

    typedef std::map< ui64, actor_t >               actors_t;

    typedef std::map< std::string, actor_info_t >   global_t;

    typedef std::map< ui64, ask_data_t* >           ask_map_t;

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
    /// -
    void         register_hook(remote_hook_t* const hook);

private:
    static void do_client_commands(command_event_t* const ev);
    static void do_server_commands(command_event_t* const ev);

    void do_send_message(command_event_t* const ev, bool is_client);

private:
    core::mutex_t   m_cs;
    actors_t        m_actors;
    ask_map_t       m_asks;
    global_t        m_registered;
    core::event_t   m_event_getid;
    volatile ui64   m_last;
    /// Генератор идентификаторов для объектов
    ui64            m_counter;
    remote_hook_t*  m_hook;

    transport_t     m_transport;
};

} // namespace remote

} // namespace acto

#endif // acto_remote_client_h_495808c30ca7465eb25ec855541ff6e8
