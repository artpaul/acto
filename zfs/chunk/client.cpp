
#include <assert.h>
#include <stdio.h>

#include <port/pointers.h>

#include "chunk.h"

//------------------------------------------------------------------------------
bool client_handler_t::is_allowed(file_info_t* file, sid_t client) {
    fprintf(stderr, "is_allowed for: %Zu\n", (size_t)client);
    std::vector<master_pending_t*>::iterator i = file->pendings.begin();

    while (i != file->pendings.end()) {
        if ((*i)->client == client) {
            file->pendings.erase(i);
            return true;
        }
        ++i;
    }
    return false;
}

void client_handler_t::on_connected(acto::remote::message_channel_t* const mc, void* param) {
    fprintf(stderr, "client connected...\n");

    m_sid     = 0;
    m_channel = mc;
    clients[m_sid] = this;
}

void client_handler_t::on_disconnected() {
    fprintf(stderr, "DoClientDisconnected\n");

    const client_map_t::iterator i = clients.find(m_sid);
    if (i != clients.end())
        clients.erase(i);
}

void client_handler_t::on_message(const acto::remote::message_t* msg) {
    fprintf(stderr, "client on_message : %s\n", rpc_command_string(msg->code));
    // -
    switch (msg->code) {
    case RPC_APPEND:
        {
            const TAppendRequest* req  = (const TAppendRequest*)msg->data;
            const char*           data = (const char*)msg->data + sizeof(TAppendRequest);
            // -
            const file_map_t::iterator i = ::files.find(req->stream);
            if (i != ::files.end()) {
                fwrite(data, 1, req->bytes, i->second->data);
                fflush(i->second->data);
            }
            // -
            TMessage resp;
            resp.size  = sizeof(resp);
            resp.code  = RPC_APPEND;
            resp.error = 0;
            m_channel->send_message(&resp, sizeof(resp));
        }
        break;
    case RPC_OPENFILE:
        {
            const TOpenChunkRequest* req = (const TOpenChunkRequest*)msg->data;
            std::map<fileid_t, file_info_t*>::iterator i;
            // -
            // -
            {
                acto::core::MutexLocker    lock(guard);

                if ((i = ::files.find(req->stream)) != ::files.end()) {
                    file_info_t* const file = i->second;
                    // проверить разрешил ли master открывать данный файл клиенту
                    if (is_allowed(file, req->client)) {
                        m_files[file->uid] = file;
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
                        fprintf(stderr, "access denided\n"); // Доступ к данному файлу запрещен
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
        fprintf(stderr, "client close\n");
        break;
    case RPC_READ:
        {
            const TReadReqest* req = (const TReadReqest*)msg->data;

            const file_map_t::iterator i = ::files.find(req->stream);
            if (i != ::files.end()) {
                char            data[DEFAULT_FILE_BLOCK];
                TReadResponse   rr;
                int rval = fread(data, 1, DEFAULT_FILE_BLOCK, i->second->data);
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
                    return;
                }
            }
            TReadResponse    rr;
            rr.size   = sizeof(rr);
            rr.code   = RPC_READ;
            rr.error  = ERPC_FILE_NOT_EXISTS;
            rr.stream = req->stream;
            rr.bytes  = 0;
            rr.crc    = 0;
            rr.futher = false;
            msg->channel->send_message(&rr, sizeof(rr));
        }
        break;
    }
}
