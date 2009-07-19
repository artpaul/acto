
#include <assert.h>
#include <memory.h>

#include <system/mutex.h>
#include <remote/libsocket/libsocket.h>

#include "transport.h"

namespace acto {

namespace remote {

/**
 */
struct network_node_t {
    void*   owner;
    int     fd;
};

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


/** */
class transport_t::impl {
    core::mutex_t   m_cs;
    host_map_t      m_hosts;
    handler_data_t  m_client_handler;
    handler_data_t  m_server_handler;
    int             m_fd_server;

private:
    //-------------------------------------------------------------------------
    static void do_client_input(int s, SOEVENT* const ev) {
       impl* const pthis = static_cast< impl* >(ev->param);

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
                        network_node_t* node = NULL;
                        {
                            for (host_map_t::iterator i = pthis->m_hosts.begin(); i != pthis->m_hosts.end(); ++i) {
                                if ((*i).second.fd == s) {
                                    node = (*i).second.node;
                                    break;
                                }
                            }
                        }

                        command_event_t ev;
                        ev.cmd    = cmd;
                        ev.node   = node;
                        ev.stream = &stream;
                        ev.param  = pthis->m_client_handler.param;

                        pthis->m_client_handler.callback(&ev);
                    }
                }
            }
            break;
        }
        // -
        so_pending(s, SOEVENT_READ, 60, &impl::do_client_input, ev->param);
    }
    //-------------------------------------------------------------------------
    static void do_host_connected(int s, SOEVENT* const ev) {
        impl* const pthis = static_cast< impl* >(ev->param);

        switch (ev->type) {
        case SOEVENT_ACCEPTED:
            {
                const int       fd   = ((SORESULT_LISTEN*)ev->data)->client;
                network_node_t* node = new network_node_t();
                // -
                node->owner = pthis;
                node->fd    = fd;
                // -
                so_pending(fd, SOEVENT_READ, 60, &impl::do_server_input, node);
            }
            break;
        case SOEVENT_CLOSED:
            pthis->m_fd_server = 0;
            break;
        }
    }
    //-----------------------------------------------------------------------------
    static void do_server_input(int s, SOEVENT* const ev) {
        network_node_t* const pnode = static_cast< network_node_t* >(ev->param);
        impl*           const pthis = static_cast< impl* >(pnode->owner);

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

                        command_event_t ev;
                        ev.cmd    = cmd;
                        ev.node   = pnode;
                        ev.stream = &stream;
                        ev.param  = pthis->m_server_handler.param;

                        pthis->m_server_handler.callback(&ev);
                    }
                }
            }
            break;
        }
        // -
        so_pending(s, SOEVENT_READ, 60, &impl::do_server_input, ev->param);
    }

public:
    /// Установить обработчик команды
    void client_handler(handler_t callback, void* param) {
        m_client_handler.callback = callback;
        m_client_handler.param    = param;
    }
    /// -
    void server_handler(handler_t callback, void* param) {
        m_server_handler.callback = callback;
        m_server_handler.param    = param;
    }
    /// Установить соединение с удаленным узлом
    network_node_t* connect(const char* host, const int port) {
        const std::string    host_str = std::string(host);
        host_map_t::iterator i        = m_hosts.find(host_str);

        if (i != m_hosts.end()) {
            return (*i).second.node;
        }
        else {
            // Установить подключение
            const int fd = so_socket(SOCK_STREAM);

            if (fd != -1) {
                if (so_connect(fd, inet_addr("127.0.0.1"), port) != 0)
                    so_close(fd);
                else {
                    remote_host_t   data;
                    network_node_t* node = new network_node_t();

                    data.name = host_str;
                    data.fd   = fd;
                    data.node = node;
                    data.port = port;
                    m_hosts[host_str] = data;
                    // -
                    node->fd = fd;
                    // -
                    so_pending(fd, SOEVENT_READ, 60, &do_client_input, this);
                    // -
                    return node;
                }
            }
        }
        return 0;
    }
    /// Открыть узел для доступа из сети
    void open_node(int port) {
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
};



///////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
transport_msg_t::transport_msg_t()
    : m_data(new char[1024])
    , m_size(0)
{
    // -
}

transport_msg_t::~transport_msg_t() {
    // -
}
//-----------------------------------------------------------------------------
void transport_msg_t::write(const void* data, size_t size) {
    memcpy(m_data.get() + m_size, data, size);
    m_size += size;
}
//-----------------------------------------------------------------------------
void transport_msg_t::send(network_node_t* const target) const {
    so_sendsync(target->fd, m_data.get(), m_size);
}



///////////////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
transport_t::transport_t()
    : m_pimpl(new impl())
{
    so_init();
}
//-----------------------------------------------------------------------------
transport_t::~transport_t() {
    so_terminate();
}
//-----------------------------------------------------------------------------
void transport_t::client_handler(handler_t callback, void* param) {
    m_pimpl->client_handler(callback, param);
}
//-----------------------------------------------------------------------------
void transport_t::server_handler(handler_t callback, void* param) {
    m_pimpl->server_handler(callback, param);
}
//-----------------------------------------------------------------------------
network_node_t* transport_t::connect(const char* host, const int port) {
    return m_pimpl->connect(host, port);
}
//-----------------------------------------------------------------------------
void transport_t::open_node(int port) {
    m_pimpl->open_node(port);
}
//-----------------------------------------------------------------------------
void transport_t::send_message(network_node_t* const target, const transport_msg_t& msg) {
    msg.send(target);
}

} // namespace remote

} // namespace acto
