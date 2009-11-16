
#include <assert.h>
#include <stdio.h>

#include <port/pointers.h>

#include "chunk.h"

//------------------------------------------------------------------------------
static bool IsAllowed(TFileInfo* file, sid_t client) {
    printf("isAllowed for: %Zu\n", (size_t)client);
    std::vector<MasterPending*>::iterator i = file->pendings.begin();

    while (i != file->pendings.end()) {
        printf("\t%Zu\n", (size_t)(*i)->client);
        if ((*i)->client == client) {
            file->pendings.erase(i);
            return true;
        }
        ++i;
    }
    return false;
}

void TClientHandler::on_connected(acto::remote::message_channel_t* const mc, void* param) {
    fprintf(stderr, "client connected...\n");

    this->sid     = 0;
    this->channel = mc;
    clients[this->sid] = this;
}

void TClientHandler::on_disconnected() {
    printf("DoClientDisconnected\n");

    const ClientsMap::iterator i = clients.find(this->sid);
    if (i != clients.end()) {
        clients.erase(i);
    }
    // -
    delete this;
}

void TClientHandler::on_message(const acto::remote::message_t* msg) {
    printf("client on_message : %s\n", RpcCommandString(msg->code));
    // -
    switch (msg->code) {
    case RPC_APPEND:
        {
            const TAppendRequest* req  = (const TAppendRequest*)msg->data;
            const char*           data = (const char*)msg->data + sizeof(TAppendRequest);
            // -
            const FilesMap::iterator i = ::files.find(req->stream);
            if (i != ::files.end()) {
                fwrite(data, 1, req->bytes, i->second->data);
                fflush(i->second->data);
            }
            // -
            TMessage resp;
            resp.size  = sizeof(resp);
            resp.code  = RPC_APPEND;
            resp.error = 0;
            this->channel->send_message(&resp, sizeof(resp));
        }
        break;
    case RPC_OPENFILE:
        {
            const TOpenChunkRequest* req = (const TOpenChunkRequest*)msg->data;
            std::map<fileid_t, TFileInfo*>::iterator i;
            // -
            // -
            {
                acto::core::MutexLocker    lock(guard);

                if ((i = ::files.find(req->stream)) != ::files.end()) {
                    printf("2\n");
                    TFileInfo* const file = i->second;
                    // проверить разрешил ли master открывать данный файл клиенту
                    if (IsAllowed(file, req->client)) {
                        this->files[file->uid] = file;
                        const char* mode = 0;
                        // -
                        if (req->mode & ZFS_CREATE)
                            mode = "w";
                        else
                            mode = "r";
                        // -
                        char        buf[128];
                        sprintf(buf, "data/%Lu", (long long unsigned int)file->uid);
                        FILE* fd = fopen(buf, mode);
                        if (fd != 0) {
                            file->data = fd;
                            // -
                            TOpenChunkResponse   rsp;
                            rsp.size  = sizeof(rsp);
                            rsp.code  = RPC_OPENFILE;
                            rsp.file  = file->uid;
                            rsp.error = 0;
                            msg->channel->send_message(&rsp, sizeof(rsp));
                            return;
                        }
                    }
                    else
                        printf("access denided\n"); // Доступ к данному файлу запрещен
                }
            }
            // -
            TOpenChunkResponse   rsp;
            rsp.size  = sizeof(rsp);
            rsp.code  = RPC_OPENFILE;
            rsp.error = 1;
            rsp.file  = req->stream;

            msg->channel->send_message(&rsp, sizeof(rsp));
        }
        break;
    case RPC_CLOSE:
        printf("client close\n");
        break;
    case RPC_READ:
        {
            const TReadReqest* req = (const TReadReqest*)msg->data;

            const FilesMap::iterator i = ::files.find(req->stream);
            if (i != ::files.end()) {
                char            data[255];
                TReadResponse    rr;
                int rval = fread(data, 1, 255, i->second->data);
                if (rval > 0) {
                    rr.size   = sizeof(TReadResponse) + rval;
                    rr.code   = RPC_READ;
                    rr.error  = 0;
                    rr.stream = i->first;
                    rr.crc    = 0;
                    rr.bytes  = rval;
                    rr.futher = false;
                    msg->channel->send_message(&rr,  sizeof(rr));
                    msg->channel->send_message(data, rval);
                    //send(s, &rr, sizeof(rr), 0);
                    //send(s, data, rr.size, 0);
                    return;
                }
            }
            TReadResponse    rr;
            rr.size   = sizeof(rr);
            rr.code   = RPC_READ;
            rr.error  = 1;
            rr.stream = req->stream;
            rr.bytes  = 0;
            rr.crc    = 0;
            rr.futher = false;
            msg->channel->send_message(&rr, sizeof(rr));
        }
        break;
    }
}
