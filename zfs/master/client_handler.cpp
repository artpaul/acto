
#include <port/fnv.h>

#include "master.h"

static TChunk* chooseChunkForFile() {
    TChunkMap::iterator i = chunks.find(1);
    // -
    if (i != chunks.end()) {
        return i->second;
    }
    return 0;
}

void TClientHandler::SendCommonResponse(acto::remote::message_channel_t* mc, ui16 cmd, i16 err) {
    TMessage resp;
    resp.size  = sizeof(resp);
    resp.code  = cmd;
    resp.error = err;

    mc->send_message(&resp, sizeof(resp));
}

void TClientHandler::SendOpenError(acto::remote::message_channel_t* mc, ui64 uid, int error) {
    TOpenResponse  rsp;
    rsp.size   = sizeof(rsp);
    rsp.code   = RPC_OPENFILE;
    rsp.error  = error;
    rsp.stream = uid;

    mc->send_message(&rsp, sizeof(rsp));
}

void TClientHandler::ClientConnect(const acto::remote::message_t* msg) {
    const TMasterSession*   req = (const TMasterSession*)msg->data;
    TMasterSession          rsp = *req;
    // -
    // -
    if (req->sid == 0)
        this->sid = rsp.sid = ++client_id;
    // -
    ctx.clients[this->sid] = this;
    // -
    this->channel->send_message(&rsp, sizeof(rsp));
}

void TClientHandler::OpenFile(const acto::remote::message_t* msg) {
    const OpenRequest*  req = (const OpenRequest*)msg->data;
    const char*         name = (const char*)msg->data + sizeof(OpenRequest);
    //
    const fileid_t uid  = fnvhash64(name, req->length);
    TFileNode*     node = NULL;

    if (this->files.find(uid) != this->files.end())
        return SendOpenError(this->channel, uid, ERPC_FILE_BUSY);

    // -
    if (req->mode & ZFS_CREATE) {
        // Ошибка, если файл создаётся, но не указан режим записи
        if ((req->mode & ZFS_WRITE) == 0 && (req->mode & ZFS_APPEND) == 0)
            return SendOpenError(this->channel, uid, ERPC_BADMODE);
        // -
        int error = ctx.tree.OpenFile(name, req->length, LOCK_WRITE, (req->mode & ZFS_DIRECTORY) ? ntDirectory : ntFile, true, &node);

        if (error != 0)
            return SendOpenError(this->channel, uid, ERPC_FILEEXISTS);

        TChunk* const chunk = chooseChunkForFile();

        // Выбрать узел для хранения данных
        if (chunk == 0)
            SendOpenError(this->channel, uid, ERPC_OUTOFSPACE);
        else {
            AllocateSpace  msg;
            msg.code   = RPC_ALLOCATE;
            msg.size   = sizeof(AllocateSpace);
            msg.client = req->client;
            msg.fileid = uid;
            msg.lease  = 180;
            // -
            chunk->channel->send_message(&msg, sizeof(msg));
        }
        return;
    }
    else {
        int error = ctx.tree.OpenFile(name, req->length, LOCK_READ, ntFile, false, &node);

        if (error != 0) {
            SendOpenError(this->channel, uid, ERPC_FILE_NOT_EXISTS);
            return;
        }
        else {
            assert(node != 0);
            // Получить карту расположения файла по секторам
            // Послать узлам команду открытия файла
            // Отправить карту клиенту
            // -
            for (size_t i = 0; i < node->chunks.size(); ++i) {
                AllocateSpace  msg;
                msg.code   = RPC_ALLOWACCESS;
                msg.size   = sizeof(AllocateSpace);
                msg.client = req->client;
                msg.fileid = node->uid;
                msg.lease  = 180;
                // -
                node->chunks[i]->channel->send_message(&msg, sizeof(msg));
            }
            return;
        }
    }
    SendOpenError(this->channel, uid, ERPC_GENERIC);
}

void TClientHandler::CloseFile(const acto::remote::message_t* msg) {
    const CloseRequest* req = (const CloseRequest*)msg->data;
    // -
    {
        acto::core::MutexLocker lock(guard);

        this->files.erase(req->stream);
        // !!! Отметить файл как закрытый
        if (TFileNode* node = ctx.tree.FileById(req->stream)) {
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
    SendCommonResponse(this->channel, RPC_CLOSE, 0);
}

void TClientHandler::CloseSession(const acto::remote::message_t* msg) {
    const ClientCloseSession*  req = (const ClientCloseSession*)msg->data;

    const ClientMap::iterator i = ctx.clients.find(req->client);

    if (i != ctx.clients.end())
        i->second->closed = true;
    // -
    printf("close session: %Zu\n", (size_t)req->client);
}

void TClientHandler::on_disconnected(void* param) {
    printf("DoClientDisconnected\n");

    const ClientMap::iterator i = ctx.clients.find(this->sid);
    if (i != ctx.clients.end())
        ctx.clients.erase(i);

    // Закрыть все открытые файлы
    if (!this->files.empty()) {
        //for (TClientSession::TFiles::iterator i = cs->files.begin(); i != cs->files.end(); ++i)
            //CloseFile(i->second, internal = true);
    }
    //
    if (this->closed) {
        // -
    }
    else {
        // Соединение с клиентом было потеряно.
        // Возможно это временная ошибка.
        // Необходимо подождать некоторое время восстановаления сессии

    }
    delete this;
}

void TClientHandler::on_message(const acto::remote::message_t* msg, void* param) {
    printf("client on_message %s\n", RpcCommandString(msg->code));

    switch (msg->code) {
    case RPC_CLIENT_CONNECT: ClientConnect(msg);
        break;
    case RPC_OPENFILE:       OpenFile(msg);
        break;
    case RPC_CLOSE:          CloseFile(msg);
        break;
    case RPC_CLOSESESSION:   CloseSession(msg);
        break;
    }
};
