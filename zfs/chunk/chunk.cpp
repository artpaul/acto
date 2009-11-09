
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

#include "chunk.h"

struct Context {
    int         master;
    int         clientListen;
    fileid_t    uid;
    bool        active;
};


/// Состояние сервера
static struct Context   ctx = {0, 0, 0, true};
///
ClientsMap          clients;
FilesMap            files;
acto::core::mutex_t guard;

void doClientOpen(int s, const RpcHeader* const hdr, void* param);
void doClientRead(int s, const RpcHeader* const hdr, void* param);
void doClientAppend(int s, const RpcHeader* const hdr, void* param);
void DoClientClose(int s, const RpcHeader* const hdr, void* param);
void doClientDisconnected(int s, void* param);

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
        CommandChannel* const ch = new CommandChannel();
        // -
        ch->registerHandler(RPC_APPEND,   &doClientAppend);
        ch->registerHandler(RPC_OPENFILE, &doClientOpen);
        ch->registerHandler(RPC_READ,     &doClientRead);
        ch->registerHandler(RPC_CLOSE,    &DoClientClose);

        ch->onDisconnected(&doClientDisconnected);

        ch->activate(data->client, info);
    }
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------------
static void doAllocateSpace(int s, const RpcHeader* const hdr, void* param) {
    AllocateSpace   req;
    bool            good = false;
    // -
    so_readsync(s, &req, sizeof(req), 10);
    {
        acto::core::MutexLocker lock(guard);
        // -
        if (files.find(req.fileid) != files.end())
            printf("file already exists\n");
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
            good = true;
        }
    }

    {
        AllocateResponse    rsp;
        rsp.good = good;
        send(s, &rsp, sizeof(rsp), 0);
    }
}
//------------------------------------------------------------------------------
/// Команда от мастер-сервера разрешающая открывать файл
static void doMasterOpen(int s, const RpcHeader* const hdr, void* param) {
    AllocateSpace       req;
    bool                good = false;
    FilesMap::iterator  i;
    // -
    so_readsync(s, &req, sizeof(req), 10);
    {
        acto::core::MutexLocker lock(guard);

        if ((i = files.find(req.fileid)) == files.end())
            printf("file not exists\n");
        else {
            TFileInfo* const     info = i->second;
            MasterPending* const mp   = new MasterPending();
            // -
            mp->client   = req.client;
            mp->file     = req.fileid;
            mp->deadline = time(0) + req.lease;

            info->pendings.push_back(mp);
            good = true;
        }
    }

    {
        AllocateResponse    rsp;
        rsp.good = good;
        send(s, &rsp, sizeof(rsp), 0);
    }
}
//------------------------------------------------------------------------------
///
static void doMaster(int s, SOEVENT* const ev) {
    if (ev->type == SOEVENT_READ) {
        so_readsync(s, &ctx.uid, sizeof(ctx.uid), 5);
        printf("new id: %Zu\n", (size_t)ctx.uid);
        // -
        CommandChannel* const ch = new CommandChannel();
        // -
        ch->registerHandler(RPC_ALLOCATE,    &doAllocateSpace);
        ch->registerHandler(RPC_ALLOWACCESS, &doMasterOpen);
        ch->activate(s, 0);
    }
}
//------------------------------------------------------------------------------
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
            ChunkConnecting    req;
            req.code = RPC_NODECONNECT;
            req.size = sizeof(req);
            req.uid  = 1;
            req.freespace = 1 << 30;
            // -
            so_pending(ctx.master, SOEVENT_READ, 10, &doMaster, 0);

            if (send(ctx.master, &req, sizeof(req), 0) == -1)
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

static void handleInterrupt(int sig) {
    printf("interrupted...\n");
    //so_close(ctx.chunkListen);
    //so_close(ctx.clientListen);
    so_terminate();
    exit(0);
}

int main() {
    struct sigaction sa;
    sigemptyset (&sa.sa_mask);
    sa.sa_flags = 0;

    /* Register the handler for SIGINT. */
    sa.sa_handler = handleInterrupt;
    sigaction (SIGINT, &sa, 0);

    so_init();
    run();
    so_terminate();
    return 0;
}
