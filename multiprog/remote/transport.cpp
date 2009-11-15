
#include <assert.h>
#include <memory.h>
#include <stdio.h>

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
class mem_stream_t : public stream_t {
    generics::array_ptr< char >     m_data;
    size_t                          m_size;
    size_t                          m_pos;

public:
    mem_stream_t(char* data, size_t size)
        : m_data(data)
        , m_size(size)
        , m_pos(0)
    {
    }

public:
    void copy(int fd) {
        so_readsync(fd, m_data.get(), m_size, 15);
    }

    virtual size_t read (void* buf, size_t size) {
        memcpy(buf, m_data.get() + m_pos, size);
        m_pos += size;
        return size;
    }

    virtual void write(const void* buf, size_t size) {
        // -
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
                ui32 len = 0;

                so_readsync(s, &len, sizeof(len), 15);
                if (so_readsync(s, &cmd, sizeof(cmd), 15) == sizeof(cmd)) {
                    mem_stream_t ms(new char[len], len - sizeof(cmd));

                    ms.copy(s);

                    if (pthis->m_client_handler.callback != 0) {
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
                        ev.cmd       = cmd;
                        ev.node      = node;
                        ev.data_size = len - sizeof(cmd);
                        ev.stream    = &ms;
                        ev.param     = pthis->m_client_handler.param;

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
                ui32 len = 0;

                so_readsync(s, &len, sizeof(len), 15);
                if (so_readsync(s, &cmd, sizeof(cmd), 15) == sizeof(cmd)) {
                    mem_stream_t ms(new char[len], len - sizeof(cmd));

                    ms.copy(s);

                    if (pthis->m_server_handler.callback != 0) {
                        command_event_t ev;
                        ev.cmd       = cmd;
                        ev.node      = pnode;
                        ev.data_size = len - sizeof(cmd);
                        ev.stream    = &ms;
                        ev.param     = pthis->m_server_handler.param;

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
    ui32 size = m_size;
    so_sendsync(target->fd, &size, sizeof(size));
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
network_node_t* transport_t::connect(const char* host, const int port, handler_t cb, void* param) {
    m_pimpl->client_handler(cb, param);
    return m_pimpl->connect(host, port);
}
//-----------------------------------------------------------------------------
void transport_t::open_node(int port, handler_t cb, void* param) {
    m_pimpl->server_handler(cb, param);
    m_pimpl->open_node(port);
}
//-----------------------------------------------------------------------------
void transport_t::send_message(network_node_t* const target, const transport_msg_t& msg) {
    msg.send(target);
}



///////////////////////////////////////////////////////////////////////////////
message_channel_t::message_channel_t(message_base_t* owner, int s)
    : m_handler(0)
    , m_owner(owner)
    , m_param(0)
    , m_socket(s)
{
}

void message_channel_t::close() {
    // -
}

void message_channel_t::send_message(const void* data, size_t len) {
    so_sendsync(m_socket, data, len);
}

void message_channel_t::set_handler(message_handler_t* cb, void* param) {
    m_handler = cb;
    m_param   = param;
}

///////////////////////////////////////////////////////////////////////////////
void message_base_t::do_read_message(int s, SOEVENT* const ev) {
    message_channel_t* ch = static_cast<message_channel_t*>(ev->param);

    switch (ev->type) {
    case SOEVENT_CLOSED:
        if (ch->m_handler)
            ch->m_handler->on_disconnected(ch->m_param);
        // -
        ch->m_owner->channel_closed(ch);
        delete ch;
        return;
    case SOEVENT_TIMEOUT:
        break;
    case SOEVENT_READ:
        {
            message_header_t    hdr;

            const int rval = recv(s, &hdr, sizeof(hdr), MSG_PEEK);

            if (rval == sizeof(hdr)) {
                generics::array_ptr<char>   buf(new char[hdr.size]);

                if (so_readsync(s, buf.get(), hdr.size, 30) == hdr.size) {
                    if (ch->m_handler) {
                        message_t msg;

                        msg.size    = hdr.size;
                        msg.code    = hdr.code;
                        msg.error   = hdr.error;
                        msg.data    = buf.get();
                        msg.channel = ch;

                        ch->m_handler->on_message(&msg, ch->m_param);
                    }
                }
            }
        }
        break;
    }
    so_pending(s, SOEVENT_READ, 10, &message_base_t::do_read_message, ev->param);
}

///////////////////////////////////////////////////////////////////////////////
message_server_t::message_server_t()
    : m_param(0)
    , m_socket(0)
{
}
//-----------------------------------------------------------------------------
message_server_t::~message_server_t() {
    if (m_socket != 0)
        so_close(m_socket);
}

//-----------------------------------------------------------------------------
int message_server_t::open(const char* ip, int port, connected_handler_t cb, void* param) {
    const int s = so_socket(SOCK_STREAM);

    if (s == -1) {
        return -1;
    }
    else {
        so_listen(s, inet_addr(ip), port, 5, &message_server_t::do_connected, this);
    }

    m_handler = cb;
    m_param   = param;
    m_socket  = s;
}

//-----------------------------------------------------------------------------
void message_server_t::do_connected(int s, SOEVENT* const ev) {
    switch (ev->type) {
    case SOEVENT_ACCEPTED:
        {
            SORESULT_LISTEN*  data  = (SORESULT_LISTEN*)ev->data;
            message_server_t* pthis = static_cast<message_server_t*>(ev->param);

            message_channel_t* mc = new message_channel_t(pthis, data->client);
            pthis->m_channels.insert(mc);

            so_pending(data->client, SOEVENT_READ, 10, &message_base_t::do_read_message, mc);

            if (pthis->m_handler)
                pthis->m_handler(mc, pthis->m_param);
        }
        break;
    case SOEVENT_CLOSED:
        return;
    case SOEVENT_TIMEOUT:
        break;
    }
}

} // namespace remote

} // namespace acto
