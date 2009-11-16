
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
struct TChunk {
    typedef acto::remote::message_channel_t msg_channel_t;

    ui64            uid;            //
    sockaddr_in     ip;             // Node ip
    int             clientport;     //
    TChunk*         slave;          //
    msg_channel_t*  channel;
    int             available : 1;  //
};


typedef std::map<ui64, TChunk*>                 TChunkMap;
typedef std::map<sid_t, class TClientHandler*>  ClientMap;


/**
 * Обработчик сообщений от узлов-данных
 */
class TChunkHandler : public acto::remote::message_handler_t {
private:
    void NodeConnect    (const acto::remote::message_t* msg);
    void NodeAllocated  (const acto::remote::message_t* msg);
    void NodeAllowAccess(const acto::remote::message_t* msg);

public:
    virtual void on_connected(acto::remote::message_channel_t* const, void* param);
    virtual void on_disconnected();
    virtual void on_message(const acto::remote::message_t* msg);

public:
    TChunk*     chunk;  //
};

/**
 */
class TClientHandler : public acto::remote::message_handler_t {
private:
    void SendCommonResponse(acto::remote::message_channel_t* mc, ui16 cmd, i16 err);
    void SendOpenError     (acto::remote::message_channel_t* mc, ui64 uid, int error);

    void ClientConnect(const acto::remote::message_t* msg);
    void OpenFile     (const acto::remote::message_t* msg);
    void CloseFile    (const acto::remote::message_t* msg);
    void CloseSession (const acto::remote::message_t* msg);
public:
    virtual void on_connected(acto::remote::message_channel_t* const, void* param);
    virtual void on_disconnected();
    virtual void on_message(const acto::remote::message_t* msg);

public:
    typedef acto::remote::message_channel_t     msg_channel_t;
    typedef std::map<fileid_t, TFileNode*>      TFiles;

    sid_t           sid;        // Уникальный идентификатор сессии
    sockaddr_in     addr;       //
    int             s;
    // Список открытых/заблокированных файлов
    TFiles          files;
    msg_channel_t*  channel;
    bool            closed;     // Флаг штатного закрытия сессии
};

/** Server data context */
class TMasterServer {
public:
    void Run();

public:
    TFileDatabase       tree;

    /// Таблица подключённых клиентов
    ClientMap           clients;
    /// Таблица подключенных узлов-данных
    TChunkMap           chunks;

    acto::remote::message_server_t<TChunkHandler>   chunk_net;
    acto::remote::message_server_t<TClientHandler>  client_net;
};


TChunk* chunkById(const ui64 uid);

extern ui64                 chunkId;
extern acto::core::mutex_t  guard;
extern TMasterServer        ctx;
extern ui64                 client_id;


#endif // masterserver_h__
