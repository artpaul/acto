
#ifndef acto_remote_transport_h_20d942d7a779464eb373d0bad8f10cd0
#define acto_remote_transport_h_20d942d7a779464eb373d0bad8f10cd0

#include <map>
#include <string>

#include <system/platform.h>
#include <system/mutex.h>
#include <core/serialization.h>


namespace acto {

namespace remote {

class transport_t;


/**
 */
class channel_t : public stream_t {
    int m_fd;

public:
    channel_t(int fd) : m_fd(fd) { }

public:
    virtual void write(const void* data, size_t size);

    void commit() { }
};


/** */
struct network_node_t {
    transport_t*    owner;
    int             fd;
};


struct command_event_t {
    ui16            cmd;
    /// Узел, который инициировал событие
    network_node_t* node;
    // -
    stream_t*       stream;
    // -
    void*           param;
};

/**
 */
class transport_t {
    typedef void (*handler_t)(command_event_t* const);

    /// -
    struct handler_data_t {
        handler_t   callback;
        void*       param;
    };

    /// Параметры соединения с удаленным хостом
    struct remote_host_t {
        std::string     name;
        network_node_t* node;
        int             fd;
        int             port;
    };

    typedef std::map< std::string, remote_host_t >  host_map_t;

public:
    /// Установить обработчик команды
    void            client_handler(handler_t callback, void* param);
    /// -
    void            server_handler(handler_t callback, void* param);
    /// Установить соединение с удаленным узлом
    network_node_t* connect(const char* host, const int port);
    /// Открыть узел для доступа из сети
    void            open_node(int port);
    /// -
    channel_t create_message(network_node_t* const node, const ui16 cmd);

private:
    /// -
    static void do_client_input  (int s, SOEVENT* const ev);
    /// -
    static void do_host_connected(int s, SOEVENT* const ev);
    /// -
    static void do_server_input  (int s, SOEVENT* const ev);

private:
    core::mutex_t   m_cs;
    host_map_t      m_hosts;
    handler_data_t  m_client_handler;
    handler_data_t  m_server_handler;
    int             m_fd_server;
};

} // namespace remote

} // namespace acto

#endif // acto_remote_transport_h_20d942d7a779464eb373d0bad8f10cd0
