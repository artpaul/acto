
#ifndef masterserver_h__
#define masterserver_h__

#include <assert.h>
#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <map>

#include <system/mutex.h>
#include <remote/transport.h>
#include <remote/libsocket/libsocket.h>
#include <rpc/rpc.h>
#include <rpc/channel.h>

#include "structs.h"

typedef std::map<ui64, TChunk*>             TChunkMap;
typedef std::map<sid_t, TClientSession*>    ClientMap;

/// Server data context
class TMasterServer {
public:
    static void ClientConnected(acto::remote::message_channel_t* const mc, void* param);
    static void ChunkConnected(acto::remote::message_channel_t* const mc, void* param);

    void Run();

public:
    TFileDatabase   tree;

    /// Таблица подключённых клиентов
    ClientMap       clients;

    acto::remote::message_server_t   chunk_net;
    acto::remote::message_server_t   client_net;
};


/**
 * Обработчик сообщений от узлов-данных
 */
class TChunkHandler : public acto::remote::message_handler_t {
private:
    void NodeConnect    (const acto::remote::message_t* msg, TNodeSession* ns);
    void NodeAllocated  (const acto::remote::message_t* msg, TNodeSession* ns);
    void NodeAllowAccess(const acto::remote::message_t* msg, TNodeSession* ns);

public:
    virtual void on_disconnected(void* param);
    virtual void on_message(const acto::remote::message_t* msg, void* param);
};

/**
 */
class TClientHandler : public acto::remote::message_handler_t {
private:
    void SendCommonResponse(acto::remote::message_channel_t* mc, ui16 cmd, i16 err);
    void SendOpenError     (acto::remote::message_channel_t* mc, ui64 uid, int error);

    void ClientConnect(const acto::remote::message_t* msg, TClientSession* cs);
    void OpenFile     (const acto::remote::message_t* msg, TClientSession* cs);
    void CloseFile    (const acto::remote::message_t* msg, TClientSession* cs);
    void CloseSession (const acto::remote::message_t* msg, TClientSession* cs);
public:
    virtual void on_disconnected(void* param);
    virtual void on_message(const acto::remote::message_t* msg, void* param);
};


TChunk* chunkById(const ui64 uid);

extern ui64                 chunkId;
extern acto::core::mutex_t  guard;
extern TMasterServer        ctx;
extern TChunkMap            chunks;
extern ui64                 client_id;


#endif // masterserver_h__
