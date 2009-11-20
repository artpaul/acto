
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
    acto::remote::message_server_t<client_handler_t>    client_net;
    acto::remote::message_client_t<master_handler_t>    master;

    fileid_t    uid;
    int         actual_port;
    bool        active;
public:
    void Run();
};


/// Состояние сервера
static TContext   ctx;
///
client_map_t        clients;
file_map_t          files;
acto::core::mutex_t guard;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------------
static void load_file_list(std::list<file_info_t*>& loaded) {
    struct dirent* dp;

    DIR* dir = opendir("./data");

    if (dir) {
        while ((dp = readdir(dir)) != NULL) {
            if (dp->d_type == DT_REG) {
                file_info_t* const info = new file_info_t();
                uint64_t           uid;

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

static void send_stored_files(int s, const std::list<file_info_t*>& files) {
    typedef std::list<file_info_t*>::const_iterator   TListIterator;

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

void master_handler_t::on_connected(acto::remote::message_channel_t* const mc, void* param) {
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

void master_handler_t::on_disconnected() {
    fprintf(stderr, "master disconnected\n");
}

void master_handler_t::on_message(const acto::remote::message_t* msg) {
    fprintf(stderr, "master message : %s\n", rpc_command_string(msg->code));
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
                std::list< file_info_t* >   files;

                load_file_list(files);

                printf("count : %u\n", (unsigned int)files.size());
                // -
                if (files.size() > 0)
                    send_stored_files(s, files);
            }
            */
        }
        break;
    case RPC_ALLOCATE:
        {
            const rpc_allocate_space_t* req = (const rpc_allocate_space_t*)msg->data;
            i16 error = 0;
            {
                acto::core::MutexLocker lock(guard);
                // -
                if (files.find(req->uid) != files.end()) {
                    fprintf(stderr, "file already exists\n");
                    error = ERPC_FILEEXISTS;
                }
                else {
                    file_info_t* const      info = new file_info_t();
                    master_pending_t* const mp   = new master_pending_t();
                    // -
                    mp->client   = req->client;
                    mp->file     = req->uid;
                    mp->deadline = time(0) + req->lease;
                    // -
                    info->uid = req->uid;
                    info->pendings.push_back(mp);

                    files[req->uid] = info;
                }
            }

            rpc_allocate_response_t    rsp;
            rsp.code   = RPC_ALLOCATE;
            rsp.size   = sizeof(rsp);
            rsp.error  = error;
            rsp.client = req->client;
            rsp.cid    = req->cid;
            rsp.uid    = req->uid;
            rsp.chunk  = ctx.uid;
            this->channel->send_message(&rsp, sizeof(rsp));
        }
        break;
    case RPC_ALLOWACCESS:
        {
            const rpc_allocate_space_t* req = (const rpc_allocate_space_t*)msg->data;
            i16 error = 0;

            // -
            {
                acto::core::MutexLocker lock(guard);

                file_map_t::iterator      i = files.find(req->uid);

                if (i == files.end()) {
                    fprintf(stderr, "file not exists\n");
                    error = ERPC_FILE_NOT_EXISTS;
                }
                else {
                    file_info_t* const      info = i->second;
                    master_pending_t* const mp   = new master_pending_t();
                    // -
                    mp->client   = req->client;
                    mp->file     = req->uid;
                    mp->deadline = time(0) + req->lease;

                    info->pendings.push_back(mp);
                }
            }

            rpc_allocate_response_t    rsp;

            rsp.size   = sizeof(rsp);
            rsp.code   = RPC_ALLOWACCESS;
            rsp.error  = error;
            rsp.client = req->client;
            rsp.cid    = req->cid;
            rsp.uid    = req->uid;
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
