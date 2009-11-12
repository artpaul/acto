
#include <assert.h>
#include <signal.h>
#include <stdlib.h>

#include <list>
#include <string>
#include <map>
#include <vector>

#include <system/mutex.h>
#include <port/strings.h>
#include <port/fnv.h>

#include "master.h"

#define CLIENTPORT  21581
#define CHUNKPORT   32541
#define SERVERIP    "127.0.0.1"


static TContext             ctx;
static volatile ui64        chunkId = 0;
static volatile ui64        client_id = 0;

/// Таблица подключенных узлов-данных
static TChunkMap            chunks;
/// Таблица подключённых клиентов
static ClientMap            m_clients;

static acto::core::mutex_t  guard;


//------------------------------------------------------------------------------
/// Выбрать узел для хранения данных файла
static TChunk* chooseChunkForFile() {
    TChunkMap::iterator i = chunks.find(1);
    // -
    if (i != chunks.end()) {
        return i->second;
    }
    return 0;
}
//------------------------------------------------------------------------------
static TChunk* chunkById(const ui64 uid) {
    const TChunkMap::iterator i = chunks.find(uid);

    if (i != chunks.end())
        return i->second;
    return 0;
}
//------------------------------------------------------------------------------
static void SendOpenError(int s, ui64 uid, int error) {
    OpenResponse  rsp;
    rsp.stream = uid;
    rsp.err    = error;
    so_sendsync(s, &rsp, sizeof(rsp));
}

static void SendCommonResponse(int s, ui16 cmd, ui32 err) {
    TCommonResponse resp;
    resp.size = sizeof(TCommonResponse);
    resp.code = cmd;
    resp.err  = err;

    so_sendsync(s, &resp, sizeof(resp));
}

///////////////////////////////////////////////////////////////////////////////
//                    ВЗАИМОДЕЙСТВИЕ С CHUNK-СЕРВЕРАМИ                       //
///////////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
static int SendChunkAllocate(TChunk* const chunk, sid_t client, fileid_t fid, ui32 lease) {
    AllocateSpace  req;
    req.code   = RPC_ALLOCATE;
    req.size   = sizeof(req);
    req.client = client;
    req.fileid = fid;
    req.lease  = lease;
    // -
    so_sendsync(chunk->s, &req, sizeof(req));
    {
        AllocateResponse    rsp;
        // -
        so_readsync(chunk->s, &rsp, sizeof(rsp), 5);
        return rsp.err;
    }
    return ERPC_GENERIC;
}
//-----------------------------------------------------------------------------
static int SendChunkOpen(TChunk* const chunk, sid_t client, fileid_t fid, ui32 lease) {
    AllocateSpace  req;
    req.code   = RPC_ALLOWACCESS;
    req.size   = sizeof(req);
    req.client = client;
    req.fileid = fid;
    req.lease  = lease;
    // -
    so_sendsync(chunk->s, &req, sizeof(req));
    {
        AllocateResponse    rsp;
        // -
        so_readsync(chunk->s, &rsp, sizeof(rsp), 5);
        return rsp.err;
    }
    return ERPC_GENERIC;
}

/// Инициализация соединения с узлом-данных
static void DoNodeConnecting(int s, const RpcHeader* const hdr, void* param) {
    TNodeSession* const ns = static_cast<TNodeSession*>(param);
    ChunkConnecting     req;
    // -
    int                 rval = so_readsync(s, &req, sizeof(req), 5);

    if (rval > 0) {
        TChunk* chunk = NULL;
        // -
        if (req.uid == 0) { // Подключение нового узла
            chunk = new TChunk();
            chunk->uid = ++chunkId;
        }
        else { // Восстановление подключения
            if (chunkId < req.uid)
                chunkId = req.uid;
            chunk = chunkById(req.uid);
            if (chunk == 0) {
                chunk = new TChunk();
            }
        }
        // -
        ns->chunk = chunk;
        // -
        chunk->uid       = chunkId;
        chunk->s         = s;
        chunk->available = 1;
        chunk->ip        = ns->addr;
        chunks[chunk->uid] = chunk;
        so_sendsync(s, &chunk->uid, sizeof(chunk->uid));
        // -
        printf("client id: %d\n", (int)chunk->uid);
    }
}
//-----------------------------------------------------------------------------
static void DoNodeFileTable(int s, const RpcHeader* const hdr, void* param) {
    TNodeSession* const ns = static_cast<TNodeSession*>(param);
    TFileTableMessage   msg;
    // -
    int rval = so_readsync(s, &msg, sizeof(msg), 5);

    if (rval > 0 && msg.count > 0) {
        cl::array_ptr<ui64> buf(new ui64[msg.count]);
        size_t              length = msg.count * sizeof(ui64);

        rval = so_readsync(s, buf.get(), length, 15);

        if (rval == length) {
            printf("file-table readed\n");
        }
    }
}
//-----------------------------------------------------------------------------
static void DoChunkDisconnected(int s, void* param) {
    printf("DoChunkDisconnected\n");
    //
    TNodeSession* const ns = static_cast<TNodeSession*>(param);

    if (ns->chunk) {
        ns->chunk->available = 0;
    }

    delete ns;
}

//-----------------------------------------------------------------------------
/// Happen when node server initiate connection with master server
static void DoChunkConnected(int s, SOEVENT* const ev) {
    if (ev->type == SOEVENT_ACCEPTED) {
        SORESULT_LISTEN*  data = (SORESULT_LISTEN*)ev->data;
        TNodeSession*     ns   = new TNodeSession();
        TCommandChannel*  cc   = new TCommandChannel();
        // -
        ns->s     = data->client;
        ns->addr  = data->src;
        ns->chunk = NULL;
        // -
        cc->registerHandler(RPC_NODECONNECT,    &DoNodeConnecting);
        cc->registerHandler(RPC_NODE_FILETABLE, &DoNodeFileTable);
        // -
        cc->onDisconnected(&DoChunkDisconnected);
        cc->activate(data->client, ns);
    }
    else {
        // !Error
    }
}


////////////////////////////////////////////////////////////////////////////////
//                       ВЗАИМОДЕЙСТВИЕ С КЛИЕНТАМИ                           //
////////////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------------
// Закрытие файла
static void doClose(int s, const RpcHeader* const hdr, void* param) {
    TClientSession* const cs = static_cast<TClientSession*>(param);
    CloseRequest          req;
    // -
    so_readsync(s, &req, sizeof(CloseRequest), 5);
    {
        acto::core::MutexLocker lock(guard);

        cs->files.erase(req.stream);
        // !!! Отметить файл как закрытый
        if (TFileNode* node = ctx.tree.FileById(req.stream)) {
            if (node->refs == 0)
                throw "node->refs == 0";
            node->refs--;
            //
            if (node->refs == 0) {
                printf("refs gose to zero\n");
                // файл больше ни кем не используется
                //streams.erase(i);
            }
        }
    }
    // -
    SendCommonResponse(s, RPC_CLOSE, 0);
}
//------------------------------------------------------------------------------
/// Команда открытия потока
static void DoOpen(int s, const RpcHeader* const hdr, void* param) {
    TClientSession* const   cs = static_cast<TClientSession*>(param);
    OpenRequest             req;
    cl::array_ptr<char>     buf;

    // Прочитать заголовок команды
    so_readsync(s, &req, sizeof(OpenRequest), 5);

    buf.reset(new char[req.length]);
    so_readsync(s, buf.get(), req.length, 5);

    //
    const fileid_t uid  = fnvhash64(buf.get(), req.length);
    TFileNode*     node = NULL;

    if (cs->files.find(uid) != cs->files.end())
        return SendOpenError(s, uid, ERPC_FILE_BUSY);

    // -
    if (req.mode & ZFS_CREATE) {
        // Ошибка, если файл создаётся, но не указан режим записи
        if ((req.mode & ZFS_WRITE) == 0 && (req.mode & ZFS_APPEND) == 0)
            return SendOpenError(s, uid, ERPC_BADMODE);
        // -
        int error = ctx.tree.OpenFile(buf.get(), req.length, LOCK_WRITE, (req.mode & ZFS_DIRECTORY) ? ntDirectory : ntFile, true, &node);

        if (error != 0)
            return SendOpenError(s, uid, ERPC_FILEEXISTS);

        TChunk* const chunk = chooseChunkForFile();

        // Выбрать узел для хранения данных
        if (chunk == 0)
            return SendOpenError(s, uid, ERPC_OUTOFSPACE);

        // Послать сообщение узлу о выделении места под файл
        // Можно делать в параллельном потоке, а текущий обработает
        // свою часть и будет ждать завершения этого задания
        if ((error = SendChunkAllocate(chunk, req.client, uid, 180)) != 0)
            return SendOpenError(s, uid, error);
        else {
            assert(node != 0);
            // -
            node->chunks.push_back(chunk);
            // Множество открытых файлов
            cs->files[node->uid] = node;
            // Отправить ответ клиенту
            {
                OpenResponse    rsp;

                rsp.stream = node->uid;
                rsp.err    = 0;
                rsp.nodeip = chunk->ip;
                so_sendsync(s, &rsp, sizeof(rsp));
                return;
            }
        }
    }
    else {
        int error = ctx.tree.OpenFile(buf.get(), req.length, LOCK_READ, ntFile, false, &node);

        if (error != 0)
            return SendOpenError(s, uid, ERPC_FILE_NOT_EXISTS);
        else {
            assert(node != 0);
            // Получить карту расположения файла по секторам
            // Послать узлам команду открытия файла
            // Отправить карту клиенту
            // -
            for (size_t i = 0; i < node->chunks.size(); ++i)
                SendChunkOpen(node->chunks[i], req.client, node->uid, 180);

            // Множество открытых файлов
            cs->files[uid] = node;
        }
    }

    OpenResponse rsp;

    rsp.stream = node->uid;
    rsp.err    = 0;
    rsp.nodeip = node->chunks[0]->ip;
    so_sendsync(s, &rsp, sizeof(rsp));
}
//------------------------------------------------------------------------------
static void doClientCloseSession(int s, const RpcHeader* const hdr, void* param) {
    ClientCloseSession  req;

    so_readsync(s, &req, sizeof(req), 5);
    {
        const ClientMap::iterator i = m_clients.find(req.client);

        if (i != m_clients.end())
            i->second->closed = true;
    }
    printf("close session: %Zu\n", (size_t)req.client);
}
//------------------------------------------------------------------------------
static void DoClientDisconnected(int s, void* param) {
    TClientSession* const cs = static_cast<TClientSession*>(param);

    const ClientMap::iterator i = m_clients.find(cs->sid);
    if (i != m_clients.end())
        m_clients.erase(i);

    // Закрыть все открытые файлы
    if (!cs->files.empty()) {
        //for (TClientSession::TFiles::iterator i = cs->files.begin(); i != cs->files.end(); ++i)
            //CloseFile(i->second, internal = true);
    }
    //
    if (cs->closed)
        delete cs;
    else {
        // Соединение с клиентом было потеряно.
        // Возможно это временная ошибка.
        // Необходимо подождать некоторое время восстановаления сессии
        delete cs;
    }
    printf("DoClientDisconnected\n");
    return;
}
//------------------------------------------------------------------------------
static void doClientConnected(int s, SOEVENT* const ev) {
    if (ev->type == SOEVENT_ACCEPTED) {
        SORESULT_LISTEN*  data = (SORESULT_LISTEN*)ev->data;
        sid_t             sid = 0;
        // Установить сессионное соединение с клиентом
        {
            MasterSession   req;
            // -
            so_readsync(data->client, &req, sizeof(req), 5);
            // -
            if (req.sid == 0) {
                sid = req.sid = ++client_id;
                so_sendsync(data->client, &req, sizeof(req));
            }
        }

        TClientSession* const  cs = new TClientSession();
        TCommandChannel* const ch = new TCommandChannel();
        // -
        cs->sid    = sid;
        cs->closed = false;
        m_clients[sid] = cs;
        // -
        ch->registerHandler(RPC_CLOSE,        &doClose);
        ch->registerHandler(RPC_OPENFILE,     &DoOpen);
        ch->registerHandler(RPC_CLOSESESSION, &doClientCloseSession);

        ch->onDisconnected(&DoClientDisconnected);
        // -
        ch->activate(data->client, cs);
    }
    else {
        // !!Error
    }
}

//------------------------------------------------------------------------------
static void run() {
    ctx.chunkListen  = so_socket(SOCK_STREAM);
    // Create client socket
    ctx.clientListen = so_socket(SOCK_STREAM);
    // -
    if (ctx.clientListen != -1 && ctx.chunkListen != -1) {
        // Start listening for client incoming
        so_listen(ctx.clientListen, inet_addr(SERVERIP), CLIENTPORT, 5, &doClientConnected, 0);
        // Start listening for chunk incoming
        so_listen(ctx.chunkListen,  inet_addr(SERVERIP), CHUNKPORT,  5, &DoChunkConnected, 0);

        so_loop(-1, 0, 0);
    }
}

static void handleInterrupt(int sig) {
    printf("interrupted...\n");
    so_close(ctx.chunkListen);
    so_close(ctx.clientListen);
    exit(0);
}

int main(int argc, char* argv[]) {
    // -
    //signal(SIGINT, &handleInterrupt);
    // -
    so_init();

    ctx.chunkListen  = 0;
    ctx.clientListen = 0;

    run();
    so_terminate();
    return 0;
}
