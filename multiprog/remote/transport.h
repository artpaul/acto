
#ifndef acto_remote_transport_h_20d942d7a779464eb373d0bad8f10cd0
#define acto_remote_transport_h_20d942d7a779464eb373d0bad8f10cd0

#include <map>
#include <string>

#include <generic/memory.h>

#include <system/platform.h>
#include <core/serialization.h>


namespace acto {

namespace remote {

class  transport_t;
struct network_node_t;

//
class write_buffer_t {
    generics::array_ptr< char > m_data;
    size_t                      m_pos;
    int                         m_fd;
    const ui16                  m_cmd;

public:
    write_buffer_t(int fd, const ui16 cmd)
        : m_data(new char[1024])
        , m_pos(0)
        , m_fd(fd)
        , m_cmd(cmd)
    {
    }

public:
    void write(const void* data, size_t size);

    void commit();
};


/**
 */
class transaction_t : public stream_t {
    write_buffer_t* const m_writer;

public:
    transaction_t(write_buffer_t* const buf);

public:
    ///
    virtual void write(const void* data, size_t size);
    ///
    void commit();
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
    transport_t();
    ~transport_t();

public:
    /// Установить обработчик команды
    void            client_handler(handler_t callback, void* param);
    /// Получить поток входных данных для сообщения
    // stream_t* get_package_stream(command_event_t* ev);
    /// -
    void            server_handler(handler_t callback, void* param);
    /// Установить соединение с удаленным узлом
    network_node_t* connect(const char* host, const int port);
    /// Открыть узел для доступа из сети
    void            open_node(int port);
    /// -
    transaction_t   start_transaction(network_node_t* const node, const ui16 cmd);

private:
    class impl;

    std::auto_ptr< impl >   m_pimpl;
};

} // namespace remote

} // namespace acto

#endif // acto_remote_transport_h_20d942d7a779464eb373d0bad8f10cd0
