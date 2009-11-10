
#include <assert.h>
#include <signal.h>
#include <stdlib.h>

#include <list>
#include <string>
#include <map>
#include <vector>

#include <system/mutex.h>
#include <port/strings.h>

#include "master.h"

#define CLIENTPORT  21581
#define CHUNKPORT   32541
#define SERVERIP    "127.0.0.1"


static TContext             ctx;
static volatile __uint64_t  chunkId = 0;

static __uint64_t           counter = 1;

/// Таблица подключенных узлов-данных
static ChunkMap             chunks;
/// Таблица подключённых клиентов
static ClientMap            m_clients;
/// Таблица открытых файлов
static TFileMap             streams;

static acto::core::mutex_t  guard;


//------------------------------------------------------------------------------
/// Выбрать узел для хранения данных файла
static TChunk* chooseChunkForFile() {
    ChunkMap::iterator i = chunks.find(1);
    // -
    if (i != chunks.end()) {
        return i->second;
    }
    return 0;
}
//------------------------------------------------------------------------------
static TChunk* chunkById(const __uint64_t uid) {
    const ChunkMap::iterator i = chunks.find(uid);

    if (i != chunks.end())
        return i->second;
    return 0;
}
//------------------------------------------------------------------------------
static void sendOpenError(int s, int error) {
    OpenResponse  rsp;
    rsp.stream = 0;
    rsp.err    = error;
    so_sendsync(s, &rsp, sizeof(rsp));
}

static void SendCommonResponse(int s, uint8_t cmd, uint32_t err) {
    TCommonResponse resp;
    resp.size = sizeof(TCommonResponse);
    resp.code = cmd;
    resp.err  = err;

    so_sendsync(s, &resp, sizeof(resp));
}

///////////////////////////////////////////////////////////////////////////////
//                    ВЗАИМОДЕЙСТВИЕ С CHUNK-СЕРВЕРАМИ                       //
///////////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------------
static bool SendChunkAllocate(TChunk* const chunk, sid_t client, fileid_t fid, uint32_t lease) {
    AllocateSpace  req;
    req.code   = RPC_ALLOCATE;
    req.size   = sizeof(req);
    req.client = client;
    req.fileid = fid;
    req.lease  = lease;
    // -
    send(chunk->s, &req, sizeof(req), 0);
    {
        AllocateResponse    rsp;
        // -
        so_readsync(chunk->s, &rsp, sizeof(rsp), 5);
        if (rsp.good)
            return true;
    }
    return false;
}
//------------------------------------------------------------------------------
static void sendChunkOpen(TChunk* const chunk, sid_t client, fileid_t fid, uint32_t lease) {
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
        if (rsp.good)
            return;
    }
    return;
}

//------------------------------------------------------------------------------
///
static void DoNodeConnecting(int s, const RpcHeader* const hdr, void* param) {
    TNodeSession* const ns = static_cast<TNodeSession*>(param);
    ChunkConnecting     req;
    // -
    int                 rval = so_readsync(s, &req, sizeof(req), 5);

    if (rval > 0) {
        TChunk* const chunk = new TChunk();
        // -
        if (req.uid == 0) // Подключение нового узла
            chunkId = chunkId + 1;
        else { // Восстановление подключения
            if (chunkId < req.uid)
                chunkId = req.uid;
        }
        // -
        ns->chunk = chunk;
        // -
        chunk->uid = chunkId;
        chunk->s   = s;
        chunks[chunk->uid] = chunk;
        send(s, &chunk->uid, sizeof(chunk->uid), 0);
        // -
        printf("client id: %d\n", (int)chunk->uid);
    }
}
//-----------------------------------------------------------------------------
static void DoChunkDisconnected(int s, void* param) {
    printf("DoChunkDisconnected\n");
    //
    TNodeSession* const ns = static_cast<TNodeSession*>(param);

    if (ns->chunk) {
        // -
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
        cc->registerHandler(RPC_NODECONNECT, &DoNodeConnecting);
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
static void doNewNode(cl::const_char_iterator, TFileNode* node, void*) {
    node->map = new FileInfo();
}

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
        TFileMap::iterator i = streams.find(req.stream);
        if (i != streams.end()) {
            FileInfo* const     info = i->second;
            TFileContext* const octx = info->openctx;
            // -
            if (octx == 0)
                throw "info->openctx == 0";
            octx->refs--;
            //
            if (octx->refs == 0) {
                printf("refs gose to zero\n");
                info->openctx = 0;
                delete octx;
                // файл больше ни кем не используется
                streams.erase(i);
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
    recv(s, &req, sizeof(OpenRequest), 0);

    buf.reset(new char[req.length]);
    so_readsync(s, buf.get(), req.length, 1);

    //
    // !!! Создание или открытие файлов должно осуществляться только
    //     после наложения блокировки на все наддиректории для файла.
    //

    if (req.mode & ZFS_CREATE) {
        cl::const_char_iterator ci(buf.get(), req.length);

        if (!ctx.tree.checkExisting(ci)) {
            // Ошибка, если файл создаётся, но не указан режим записи
            if ((req.mode & ZFS_WRITE) == 0 && (req.mode & ZFS_APPEND) == 0)
                return sendOpenError(s, ERPC_BADMODE);

            // Добавить информацию о файле в базу
            TFileNode* node = ctx.tree.AddPath(ci, LOCK_WRITE, (req.mode & ZFS_DIRECTORY) ? ntDirectory : ntFile);

            TChunk* const  chunk = chooseChunkForFile();
            const fileid_t uid   = ++counter;

            // Выбрать узел для хранения данных
            if (chunk == 0)
                return sendOpenError(s, ERPC_OUTOFSPACE);

            // Послать сообщение узлу о выделении места под файл
            // Можно делать в параллельном потоке, а текущий обработает
            // свою часть и будет ждать завершения этого задания
            if (!SendChunkAllocate(chunk, req.client, uid, 180))
                return sendOpenError(s, ERPC_OUTOFSPACE);
            else {
                assert(node != 0);
                // Добавить информацию о файле в базу
                FileInfo* const     info = node->map;
                TFileContext* const octx = new TFileContext();
                // -
                octx->refs  = 1;
                octx->locks = 0;
                // -
                info->uid     = uid;
                info->openctx = octx;
                info->chunks.push_back(chunk->uid);
                streams[uid]  = info;

                //node->map = info;
                // Множество открытых файлов
                cs->files[uid] = info;
                // Отправить ответ клиенту
                {
                    OpenResponse    rsp;

                    rsp.stream = uid;
                    rsp.err    = 0;
                    rsp.nodeip = chunk->ip;
                    so_sendsync(s, &rsp, sizeof(rsp));
                    return;
                }
            }
        }
        else {
            return sendOpenError(s, ERPC_FILEEXISTS);
        }
    }
    else {
        cl::const_char_iterator ci(buf.get(), req.length);

        if (TFileNode* const node = ctx.tree.findPath(ci)) {
            FileInfo* const info = node->map;
            // Получить карту расположения файла по секторам
            // Послать узлам команду открытия файла
            // Отправить карту клиенту
            // -
            for (size_t i = 0; i < info->chunks.size(); ++i) {
                sendChunkOpen(chunkById(info->chunks[i]), req.client, info->uid, 180);
            }
            streams[info->uid] = info;

            if (info->openctx == 0) {
                TFileContext* const octx = new TFileContext();
                octx->refs  = 1;
                octx->locks = 0;

                info->openctx = octx;
            }
            else
                info->openctx->refs++;

            {
                OpenResponse rsp;

                rsp.stream = info->uid;
                rsp.err    = 0;
                //rsp.nodeip = chunk->ip;
                send(s, &rsp, sizeof(rsp), 0);
                return;
            }
        }
    }
    // -
    sendOpenError(s, ERPC_GENERIC);
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
                sid = req.sid = ++counter;
                send(data->client, &req, sizeof(req), 0);
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

/*
 * Open master database from hdd
 */
static int openDatabase(const char* path) {
    return 0;
}

static void handleInterrupt(int sig) {
    printf("interrupted...\n");
    so_close(ctx.chunkListen);
    so_close(ctx.clientListen);
    exit(0);
}

int main(int argc, char* argv[]) {
    if (openDatabase("db") != 0) {
        printf("cannot open master database\n");
        return 1;
    }
    // -
    signal(SIGINT, &handleInterrupt);
    // -
    so_init();

    ctx.chunkListen  = 0;
    ctx.clientListen = 0;

    ctx.tree.onNewNode(&doNewNode);

    run();
    so_terminate();
    return 0;
}
