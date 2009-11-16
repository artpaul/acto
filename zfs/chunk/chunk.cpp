
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
    acto::remote::message_server_t<TClientHandler>  client_net;
    acto::remote::message_client_t<TMasterHandler>  master;

    fileid_t    uid;
    int         actual_port;
    bool        active;
public:
    void Run();
};


/// Состояние сервера
static TContext   ctx;
///
ClientsMap          clients;
FilesMap            files;
acto::core::mutex_t guard;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

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

void TMasterHandler::on_connected(acto::remote::message_channel_t* const mc, void* param) {
    TChunkConnecting    req;
    req.code  = RPC_NODECONNECT;
    req.size  = sizeof(req);
    req.error = 0;
    req.uid   = 0;
    req.freespace = 1 << 30;
    req.ip.sin_addr.s_addr = inet_addr(SERVERIP);
    req.port  = ctx.actual_port;
    // -
    mc->send_message(&req, sizeof(req));

    this->channel = mc;
}

void TMasterHandler::on_disconnected() {
    fprintf(stderr, "master disconnected\n");
}

void TMasterHandler::on_message(const acto::remote::message_t* msg) {
    fprintf(stderr, "master message : %s\n", RpcCommandString(msg->code));
    switch (msg->code) {
    case RPC_NODECONNECT:
        {
            const TChunkConnecting* req = (const TChunkConnecting*)msg->data;

            if (req->error != 0) {
                fprintf(stderr, "master error : %i\n", req->error);
                exit(1);
            }
            else {
                ctx.uid = req->uid;
                fprintf(stderr, "new id: %Zu\n", (size_t)ctx.uid);
            }
            /*
             // Отослать список хранящихся файлов

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
        break;
    case RPC_ALLOCATE:
        {
            const AllocateSpace* req = (const AllocateSpace*)msg->data;
            uint16_t        error = 0;
            {
                acto::core::MutexLocker lock(guard);
                // -
                if (files.find(req->fileid) != files.end()) {
                    fprintf(stderr, "file already exists\n");
                    error = ERPC_FILEEXISTS;
                }
                else {
                    TFileInfo* const     info = new TFileInfo();
                    MasterPending* const mp   = new MasterPending();
                    // -
                    mp->client   = req->client;
                    mp->file     = req->fileid;
                    mp->deadline = time(0) + req->lease;
                    // -
                    info->uid = req->fileid;
                    info->pendings.push_back(mp);

                    files[req->fileid] = info;
                }
            }

            AllocateResponse    rsp;
            rsp.code   = RPC_ALLOCATE;
            rsp.size   = sizeof(AllocateResponse);
            rsp.error  = error;
            rsp.client = req->client;
            rsp.fileid = req->fileid;
            rsp.chunk  = ctx.uid;
            this->channel->send_message(&rsp, sizeof(rsp));
        }
        break;
    case RPC_ALLOWACCESS:
        {
            const AllocateSpace* req = (const AllocateSpace*)msg->data;
            uint16_t             error = 0;

            // -
            {
                acto::core::MutexLocker lock(guard);
                FilesMap::iterator      i = files.find(req->fileid);

                if (i == files.end()) {
                    fprintf(stderr, "file not exists\n");
                    error = ERPC_FILE_NOT_EXISTS;
                }
                else {
                    TFileInfo* const     info = i->second;
                    MasterPending* const mp   = new MasterPending();
                    // -
                    mp->client   = req->client;
                    mp->file     = req->fileid;
                    mp->deadline = time(0) + req->lease;

                    info->pendings.push_back(mp);
                }
            }

            AllocateResponse    rsp;

            rsp.size   = sizeof(rsp);
            rsp.code   = RPC_ALLOWACCESS;
            rsp.error  = error;
            rsp.client = req->client;
            rsp.fileid = req->fileid;
            rsp.chunk  = ctx.uid;
            this->channel->send_message(&rsp, sizeof(rsp));
        }
        break;
    }
}

void TContext::Run() {
    int rval = client_net.open(SERVERIP, this->actual_port, 0);

    if (rval != 0) {
        fprintf(stderr, "error : %i\n", errno);
        exit(1);
    }

    if (int rval = master.connect(MASTERIP, MASTERPORT)) {
        fprintf(stderr, "cannot connect to master: %d\n", rval);
        return;
    }
    so_loop(-1, 0, 0);
}

int main(int argc, char* argv[]) {
    int port = CLIENTPORT;
    if (argc > 1)
        port = atoi(argv[1]);

    so_init();
    // Прочитать все файлы из директории data
    // -
    ctx.uid    = 0;
    ctx.actual_port = port;
    ctx.active = true;
    ctx.Run();
    so_terminate();
    return 0;
}
