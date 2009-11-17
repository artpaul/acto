
#ifndef masterserver_h__
#define masterserver_h__

#include <assert.h>
#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#include <map>

#include <system/mutex.h>
#include <remote/transport.h>
#include <remote/libsocket/libsocket.h>
#include <rpc/rpc.h>

#include "structs.h"


/** Параметры сервера-данных */
struct chunk_t {
    typedef acto::remote::message_channel_t msg_channel_t;

    ui64            uid;            //
    sockaddr_in     ip;             // Node ip
    int             clientport;     //
    chunk_t*        slave;          //
    msg_channel_t*  channel;
    int             available : 1;  //
};


typedef std::map<ui64, chunk_t*>                 chunk_map_t;
typedef std::map<sid_t, class client_handler_t*> client_map_t;


/**
 * Обработчик сообщений от узлов-данных
 */
class chunk_handler_t : public acto::remote::message_handler_t {
private:
    void node_connect     (const acto::remote::message_t* msg);
    void node_allocated   (const acto::remote::message_t* msg);
    void node_allow_access(const acto::remote::message_t* msg);

public:
    virtual void on_connected(acto::remote::message_channel_t* const, void* param);
    virtual void on_disconnected();
    virtual void on_message(const acto::remote::message_t* msg);

public:
    chunk_t*     chunk;  //
};

/**
 */
class client_handler_t : public acto::remote::message_handler_t {
private:
    void send_common_response(acto::remote::message_channel_t* mc, ui16 cmd, i16 err);
    void send_open_error     (acto::remote::message_channel_t* mc, ui64 uid, int error);

    void client_connect(const acto::remote::message_t* msg);
    void open_file     (const acto::remote::message_t* msg);
    void close_file    (const acto::remote::message_t* msg);
    void close_session (const acto::remote::message_t* msg);
public:
    virtual void on_connected(acto::remote::message_channel_t* const, void* param);
    virtual void on_disconnected();
    virtual void on_message(const acto::remote::message_t* msg);

public:
    typedef acto::remote::message_channel_t     msg_channel_t;
    typedef std::map<fileid_t, file_node_t*>    files_map_t;

    sid_t           m_sid;        // Уникальный идентификатор сессии
    sockaddr_in     m_addr;       //
    int             m_s;
    // Список открытых/заблокированных файлов
    files_map_t     m_files;
    msg_channel_t*  m_channel;
    bool            m_closed;     // Флаг штатного закрытия сессии
};

/** Server data context */
class master_server_t {
public:
    void run();

public:
    file_database_t     m_tree;

    /// Таблица подключённых клиентов
    client_map_t        m_clients;
    /// Таблица подключенных узлов-данных
    chunk_map_t         m_chunks;

    acto::remote::message_server_t<chunk_handler_t>  chunk_net;
    acto::remote::message_server_t<client_handler_t> client_net;
};


chunk_t* chunkById(const ui64 uid);

extern ui64                 chunkId;
extern acto::core::mutex_t  guard;
extern master_server_t      ctx;
extern ui64                 client_id;


#endif // masterserver_h__
