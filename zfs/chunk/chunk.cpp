
#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <dirent.h>

#include <list>

#include <port/pointers.h>

#include "chunk.h"

struct TContext {
    int         master;
    int         clientListen;
    fileid_t    uid;
    bool        active;
};


/// Состояние сервера
static TContext   ctx = {0, 0, 0, true};
///
ClientsMap          clients;
FilesMap            files;
acto::core::mutex_t guard;

void DoClientOpen(int s, const RpcHeader* const hdr, void* param);
void DoClientRead(int s, const RpcHeader* const hdr, void* param);
void DoClientAppend(int s, const RpcHeader* const hdr, void* param);
void DoClientClose(int s, const RpcHeader* const hdr, void* param);
void DoClientDisconnected(int s, void* param);

//------------------------------------------------------------------------------
/// Обработчик подключения клиентов
static void doClientConnected(int s, SOEVENT* const ev) {
    if (ev->type == SOEVENT_ACCEPTED) {
        SORESULT_LISTEN*  data = (SORESULT_LISTEN*)ev->data;
        TClientInfo* const info = new TClientInfo();
        // -
        {
            acto::core::MutexLocker lock(guard);
            // -
            info->sid = 0;
            clients[info->sid] = info;
        }
        printf("client connected...\n");
        // -
        TCommandChannel* const ch = new TCommandChannel();
        // -
        ch->registerHandler(RPC_APPEND,   &DoClientAppend);
        ch->registerHandler(RPC_OPENFILE, &DoClientOpen);
        ch->registerHandler(RPC_READ,     &DoClientRead);
        ch->registerHandler(RPC_CLOSE,    &DoClientClose);

        ch->onDisconnected(&DoClientDisconnected);

        ch->activate(data->client, info);
    }
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------------
static void doAllocateSpace(int s, const RpcHeader* const hdr, void* param) {
    AllocateSpace   req;
    uint16_t        error = 0;
    // -
    so_readsync(s, &req, sizeof(req), 10);
    {
        acto::core::MutexLocker lock(guard);
        // -
        if (files.find(req.fileid) != files.end()) {
            printf("file already exists\n");
            error = ERPC_FILEEXISTS;
        }
        else {
            TFileInfo* const     info = new TFileInfo();
            MasterPending* const mp   = new MasterPending();
            // -
            mp->client   = req.client;
            mp->file     = req.fileid;
            mp->deadline = time(0) + req.lease;
            // -
            info->uid = req.fileid;
            info->pendings.push_back(mp);

            files[req.fileid] = info;
        }
    }

    AllocateResponse    rsp;
    rsp.code   = RPC_ALLOCATE;
    rsp.size   = sizeof(AllocateResponse);
    rsp.error  = error;
    rsp.client = req.client;
    rsp.fileid = req.fileid;
    rsp.chunk  = ctx.uid;
    send(s, &rsp, sizeof(rsp), 0);
}
//------------------------------------------------------------------------------
/// Команда от мастер-сервера разрешающая открывать файл
static void doMasterOpen(int s, const RpcHeader* const hdr, void* param) {
    AllocateSpace       req;
    uint16_t            error = 0;
    FilesMap::iterator  i;
    // -
    so_readsync(s, &req, sizeof(req), 10);
    {
        acto::core::MutexLocker lock(guard);

        if ((i = files.find(req.fileid)) == files.end()) {
            printf("file not exists\n");
            error = ERPC_FILE_NOT_EXISTS;
        }
        else {
            TFileInfo* const     info = i->second;
            MasterPending* const mp   = new MasterPending();
            // -
            mp->client   = req.client;
            mp->file     = req.fileid;
            mp->deadline = time(0) + req.lease;

            info->pendings.push_back(mp);
        }
    }


    AllocateResponse    rsp;

    rsp.code  = RPC_ALLOWACCESS;
    rsp.size  = sizeof(rsp);
    rsp.error = error;
    rsp.client = req.client;
    rsp.fileid = req.fileid;
    rsp.chunk  = ctx.uid;
    send(s, &rsp, sizeof(rsp), 0);
}
//------------------------------------------------------------------------------
static void LoadFileList(std::list<TFileInfo*>& loaded) {
    struct dirent* dp;

    DIR* dir = opendir("./data");

    if (dir) {
        while ((dp = readdir(dir)) != NULL) {
            if (dp->d_type == DT_REG) {
                TFileInfo* const info = new TFileInfo();
                uint64_t         uid;

                //decode64((unsigned char*)dp->d_name, uid);

                //printf("%Lu\n", uid);

                info->uid   = uid;
                info->lease = 0;
                info->data  = 0;

                files[uid] = info;
                loaded.push_back(info);
            }
        }
        closedir(dir);
    }
}

static void SendStoredFiles(int s, const std::list<TFileInfo*>& files) {
    typedef std::list<TFileInfo*>::const_iterator   TListIterator;

    const size_t            count  = files.size();
    uint64_t                length = count * sizeof(uint64_t);
    cl::array_ptr<uint64_t> data(new uint64_t[count]);
    size_t j = 0;

    TFileTableMessage   msg;

    msg.size  = sizeof(msg) + length;
    msg.code  = RPC_NODE_FILETABLE;
    msg.uid   = ctx.uid;
    msg.count = count;

    for (TListIterator i = files.begin(); i != files.end(); ++i)
        data.get()[j++] = (*i)->uid;

    so_sendsync(s, &msg, sizeof(msg));
    so_sendsync(s, data.get(), length);
}
//------------------------------------------------------------------------------
static void DoMaster(int s, SOEVENT* const ev) {
    if (ev->type == SOEVENT_READ) {
        TChunkConnecting    msg;

        so_readsync(s, &msg, sizeof(msg), 5);

        if (msg.error != 0) {
            printf("master error : %i\n", msg.error);
            exit(1);
        }
        else {
            ctx.uid = msg.uid;
            printf("new id: %Zu\n", (size_t)ctx.uid);
            // -
            TCommandChannel* const ch = new TCommandChannel();
            // -
            ch->registerHandler(RPC_ALLOCATE,    &doAllocateSpace);
            ch->registerHandler(RPC_ALLOWACCESS, &doMasterOpen);
            ch->activate(s, 0);
        }

        // Отослать список хранящихся файлов
        /*
        {
            std::list<TFileInfo*>   files;

            LoadFileList(files);

            printf("count : %u\n", (unsigned int)files.size());
            // -
            if (files.size() > 0)
                SendStoredFiles(s, files);
        }
        */
    }
}
int run() {
    ctx.master       = so_socket(SOCK_STREAM);
    // Create client socket
    ctx.clientListen = so_socket(SOCK_STREAM);

    if (ctx.master != -1 and ctx.clientListen != -1) {
        int rval = so_connect(ctx.master, inet_addr(MASTERIP), MASTERPORT);

        if (rval != 0) {
            printf("cannot connect to master: %d\n", rval);
            return 1;
        }
        else {
            TChunkConnecting    req;
            req.code = RPC_NODECONNECT;
            req.size = sizeof(req);
            req.uid  = 0;
            req.freespace = 1 << 30;
            req.ip.sin_addr.s_addr   = inet_addr(SERVERIP);
            req.port = CLIENTPORT;
            // -
            so_pending(ctx.master, SOEVENT_READ, 10, &DoMaster, 0);

            if (so_sendsync(ctx.master, &req, sizeof(req)) == -1)
                printf("error on send: %d\n", errno);
            else
                printf("sended\n");
            // -
            so_listen(ctx.clientListen, inet_addr(SERVERIP), CLIENTPORT, 5, &doClientConnected, 0);
            // -
            so_loop(-1, 0, 0);
        }
    }
    else
        printf("so_socket error\n");
    return 0;
}

int main() {
    so_init();
    // Прочитать все файлы из директории data
    // -
    run();
    so_terminate();
    return 0;
}
