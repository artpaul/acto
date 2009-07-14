
#ifndef acto_remote_transport_h_20d942d7a779464eb373d0bad8f10cd0
#define acto_remote_transport_h_20d942d7a779464eb373d0bad8f10cd0

#include <map>
#include <string>

#include <system/platform.h>
#include <system/mutex.h>
#include <core/serialization.h>


namespace acto {

namespace remote {

/**
 */
class channel_t {
    int m_fd;

public:
    channel_t(int fd) : m_fd(fd) { }

public:
    void write(const void* data, const size_t size);
};


/**
 */
class transport_t {
    typedef void (*handler_t)(const ui16, stream_t*, void*);

    /// -
    struct handler_data_t {
        handler_t   callback;
        void*       param;
    };

    /// Параметры соединения с удаленным хостом
    struct remote_host_t {
        std::string name;
        int         fd;
        int         port;
    };

    typedef std::map< std::string, remote_host_t >  host_map_t;

public:
    /// Установить обработчик команды
    void      client_handler(handler_t callback, void* param);
    /// -
    void      server_handler(handler_t callback, void* param);
    ///
    int       connect(const char* host, const int port);
    /// -
    void      open_server(int port);
    ///
    channel_t send_command(int fd, const ui16 cmd);

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
