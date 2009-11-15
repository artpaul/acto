
#ifndef acto_remote_transport_h_20d942d7a779464eb373d0bad8f10cd0
#define acto_remote_transport_h_20d942d7a779464eb373d0bad8f10cd0

#include <map>
#include <set>
#include <string>
#include <memory>

#include <generic/memory.h>

#include <system/platform.h>
#include <core/serialization.h>

// !!!
#include <remote/libsocket/libsocket.h>


namespace acto {

namespace remote {

class  transport_t;
struct network_node_t;

/**
 */
class transport_msg_t : public stream_t {
    generics::array_ptr< char > m_data;
    size_t                      m_size;

public:
    transport_msg_t();
    ~transport_msg_t();

public:
    ///
    void         send(network_node_t* target) const;
    ///
    virtual void write(const void* data, size_t size);
};


struct command_event_t {
    ui16            cmd;
    /// Узел, который инициировал событие
    network_node_t* node;
    /// Размер потока данных
    size_t          data_size;
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
    transport_t();
    ~transport_t();

public:
    /// \brief Установить соединение с удаленным узлом
    network_node_t* connect(const char* host, const int port, handler_t cb, void* param);

    /// \brief Открыть узел для доступа из сети
    void            open_node(int port, handler_t cb, void* param);

    /// -
    void            send_message(network_node_t* const target, const transport_msg_t& msg);

private:
    class impl;

    std::auto_ptr< impl >   m_pimpl;
};

///////////////////////////////////////////////////////////////////////////////

#pragma pack(push, 4)

struct message_header_t {
    ui32        size;       // Суммарная длинна данных в запросе (+ заголовок)
    ui16        code;       // Command code
    i16         error;      // Код ошибки
};

#pragma pack(pop)


struct message_t {
    ui32        size;       // Суммарная длинна данных (включая заголовок)
    ui16        code;       // Код сообщения
    i16         error;      // Код ошибки
    void*       data;       // Буфер данных (включая заголовок)
    /// Канал, по которому пришло данное сообщение
    class message_channel_t*    channel;
};

/** */
class message_handler_t {
public:
    virtual ~message_handler_t() { };

    ///
    virtual void on_connected(message_channel_t* const, void* param) { }
    ///
    virtual void on_disconnected(void* param) { }
    ///
    virtual void on_message(const message_t* msg, void* param) { }
};

/**
 * Канал обмена сообщениями между узлами
 */
class message_channel_t {
    friend class message_base_t;

    message_handler_t*      m_handler;
    class message_base_t*   m_owner;
    void*                   m_param;
    int                     m_socket;

public:
    message_channel_t(class message_base_t* owner, int s);

    /// \brief Закрыть канал
    void close();

    /// \brief Отправить сообщение клиенту
    void send_message(const void* data, size_t len);

    ///
    void set_handler(message_handler_t* cb, void* param);
};

/** */
class message_base_t {
protected:
    ///
    static void do_read_message(int s, SOEVENT* const ev);

    virtual void channel_closed(message_channel_t* const) { }
};

/**
 * Клиент сервера сообщений
 */
template <typename T>
class message_client_t : public message_base_t {
public:
    message_client_t()
        : m_channel(0)
        , m_handler(0)
    {
    }

    ///
    message_channel_t* channel() const {
        return m_channel;
    }
    ///
    int  connect(const char* ip, int port, void* param = 0);
    ///
    bool connected() const {
        m_channel != 0;
    }

protected:
    virtual void channel_closed(message_channel_t* const) {
        m_channel = 0;
    }

private:
    message_channel_t*  m_channel;
    T*                  m_handler;
};

/**
 * Сервер сообщений
 */
class message_server_t : public message_base_t {
    /// Множество активных каналов взаимодействия с клиентами
    typedef std::set<message_channel_t*>    channels_t;
    ///
    typedef void (*connected_handler_t)(message_channel_t* const, void* param);

public:
    message_server_t();
    ~message_server_t();

    ///
    int open(const char* ip, int port, connected_handler_t cb, void* param);

protected:
    virtual void channel_closed(message_channel_t* const ch) {
        m_channels.erase(ch);
    }

private:
    static void do_connected(int s, SOEVENT* const ev);

private:
    channels_t          m_channels;
    connected_handler_t m_handler;
    void*               m_param;
    int                 m_socket;
};



template <typename T>
int message_client_t<T>::connect(const char* ip, int port, void* param) {
    if (m_channel != NULL)
        return 1;
    else {
        int s    = so_socket(SOCK_STREAM);
        int rval = so_connect(s, inet_addr(ip), port);

        if (rval != 0)
            return rval;
        else {
            m_channel = new message_channel_t(this, s);
            m_handler = new T();

            m_handler->on_connected(m_channel, param);
            m_channel->set_handler(m_handler, 0);
            // -
            so_pending(s, SOEVENT_READ, 10, &message_base_t::do_read_message, m_channel);
        }
    }
    return 0;
}

} // namespace remote

} // namespace acto

#endif // acto_remote_transport_h_20d942d7a779464eb373d0bad8f10cd0
