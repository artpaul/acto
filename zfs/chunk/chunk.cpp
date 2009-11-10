
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
static void doMaster(int s, SOEVENT* const ev) {
    if (ev->type == SOEVENT_READ) {
        so_readsync(s, &ctx.uid, sizeof(ctx.uid), 5);
        printf("new id: %Zu\n", (size_t)ctx.uid);
        // -
        TCommandChannel* const ch = new TCommandChannel();
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

#include <dirent.h>

static void LoadFileList() {
    struct dirent* dp;

    DIR* dir = opendir("./data");

    if (dir) {
        while ((dp = readdir(dir)) != NULL) {
            if (dp->d_type == DT_REG) {
                //uuid_t id;

                //if (uuid_parse(dp->d_name, id) == 0) {
                //    TFileInfo* const info = new TFileInfo();
                //    info->lease = 0;
                //    info->data = 0;
                //    uuid_copy(info->uid, id);
                //    files[uuid_hash64(id)] = info;
                //}
            }
        }
        closedir(dir);
    }
}


int main() {
    so_init();
    // Прочитать все файлы из директории data
    // -
    run();
    so_terminate();
    return 0;
}
