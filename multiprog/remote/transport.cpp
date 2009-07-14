
#include <remote/libsocket/libsocket.h>

#include "transport.h"

namespace acto {

namespace remote {

/** */
class socket_stream_t : public stream_t {
    int m_fd;

public:
    socket_stream_t(int fd) : m_fd(fd) {
    }

public:
    virtual size_t read (void* buf, size_t size) {
        return so_readsync(m_fd, buf, size, 60);
    }

    virtual void write(const void* buf, size_t size) {
        so_sendsync(m_fd, buf, size);
    }
};


///////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
void channel_t::write(const void* data, const size_t size) {
    so_sendsync(m_fd, data, size);
}


///////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
void transport_t::client_handler(handler_t callback, void* param) {
    m_client_handler.callback = callback;
    m_client_handler.param    = param;
}
//-----------------------------------------------------------------------------
void transport_t::server_handler(handler_t callback, void* param) {
    m_server_handler.callback = callback;
    m_server_handler.param    = param;
}
//-----------------------------------------------------------------------------
int transport_t::connect(const char* host, const int port) {
    const std::string    host_str = std::string(host);
    host_map_t::iterator i        = m_hosts.find(host_str);

    if (i != m_hosts.end()) {
        return (*i).second.fd;
    }
    else {
        // Установить подключение
        const int fd = so_socket(SOCK_STREAM);

        if (fd != -1) {
            if (so_connect(fd, inet_addr("127.0.0.1"), port) != 0)
                so_close(fd);
            else {
                remote_host_t data;

                data.name = host_str;
                data.fd   = fd;
                data.port = port;
                m_hosts[host_str] = data;
                // -
                so_pending(fd, SOEVENT_READ, 60, &do_client_input, this);
                // -
                return fd;
            }
        }
    }
    return 0;
}
//-----------------------------------------------------------------------------
void transport_t::open_server(int port) {
    if (m_fd_server == 0) {
        m_fd_server = so_socket(SOCK_STREAM);

        if (m_fd_server != -1)
            so_listen(m_fd_server, inet_addr("127.0.0.1"), port,  5, &do_host_connected, this);
        else {
            printf("create error\n");
            m_fd_server = 0;
        }
    }
}
//-----------------------------------------------------------------------------
channel_t transport_t::send_command(int fd, const ui16 cmd) {
    so_sendsync(fd, &cmd, sizeof(cmd));
    return channel_t(fd);
}

//-----------------------------------------------------------------------------
void transport_t::do_client_input(int s, SOEVENT* const ev) {
   transport_t* const pthis = static_cast< transport_t* >(ev->param);

    switch (ev->type) {
    case SOEVENT_CLOSED:
        // Соединение с сервером было закрыто
        printf("closed\n");
        return;
    case SOEVENT_TIMEOUT:
        break;
    case SOEVENT_READ:
        {
            ui16 cmd = 0;

            if (so_readsync(s, &cmd, sizeof(cmd), 15) == sizeof(cmd)) {
                if (pthis->m_client_handler.callback != 0) {
                    socket_stream_t stream(s);

                    pthis->m_client_handler.callback(cmd, &stream, pthis->m_client_handler.param);
                }
            }
        }
        break;
    }
    // -
    so_pending(s, SOEVENT_READ, 60, &do_client_input, ev->param);
}
//-----------------------------------------------------------------------------
void transport_t::do_host_connected(int s, SOEVENT* const ev) {
    transport_t* const pthis = static_cast< transport_t* >(ev->param);

    switch (ev->type) {
    case SOEVENT_ACCEPTED:
        so_pending(((SORESULT_LISTEN*)ev->data)->client, SOEVENT_READ, 60, &do_server_input, ev->param);
        break;
    case SOEVENT_CLOSED:
        pthis->m_fd_server = 0;
        break;
    }
}
//-----------------------------------------------------------------------------
void transport_t::do_server_input(int s, SOEVENT* const ev) {
    transport_t* const pthis = static_cast< transport_t* >(ev->param);

    switch (ev->type) {
    case SOEVENT_CLOSED:
        return;
    case SOEVENT_TIMEOUT:
        printf("time out\n");
        break;
    case SOEVENT_READ:
        {
            ui16 cmd = 0;

            if (so_readsync(s, &cmd, sizeof(cmd), 15) == sizeof(cmd)) {
                if (pthis->m_server_handler.callback != 0) {
                    socket_stream_t stream(s);

                    pthis->m_server_handler.callback(cmd, &stream, pthis->m_server_handler.param);
                }
            }
        }
        break;
    }
    // -
    so_pending(s, SOEVENT_READ, 60, &transport_t::do_server_input, ev->param);
}

} // namespace remote

} // namespace acto
