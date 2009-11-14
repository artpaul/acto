
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

void TClientHandler::ClientConnect(const acto::remote::message_t* msg, TClientSession* cs) {
    const TMasterSession*   req = (const TMasterSession*)msg->data;
    TMasterSession          rsp = *req;
    // -
    // -
    if (req->sid == 0)
        cs->sid = rsp.sid = ++client_id;
    // -
    ctx.clients[cs->sid] = cs;
    // -
    cs->channel->send_message(&rsp, sizeof(rsp));
}

void TClientHandler::OpenFile(const acto::remote::message_t* msg, TClientSession* cs) {
    const OpenRequest*  req = (const OpenRequest*)msg->data;
    const char*         name = (const char*)msg->data + sizeof(OpenRequest);
    //
    const fileid_t uid  = fnvhash64(name, req->length);
    TFileNode*     node = NULL;

    if (cs->files.find(uid) != cs->files.end())
        return SendOpenError(cs->channel, uid, ERPC_FILE_BUSY);

    // -
    if (req->mode & ZFS_CREATE) {
        // Ошибка, если файл создаётся, но не указан режим записи
        if ((req->mode & ZFS_WRITE) == 0 && (req->mode & ZFS_APPEND) == 0)
            return SendOpenError(cs->channel, uid, ERPC_BADMODE);
        // -
        int error = ctx.tree.OpenFile(name, req->length, LOCK_WRITE, (req->mode & ZFS_DIRECTORY) ? ntDirectory : ntFile, true, &node);

        if (error != 0)
            return SendOpenError(cs->channel, uid, ERPC_FILEEXISTS);

        TChunk* const chunk = chooseChunkForFile();

        // Выбрать узел для хранения данных
        if (chunk == 0)
            SendOpenError(cs->channel, uid, ERPC_OUTOFSPACE);
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
            SendOpenError(cs->channel, uid, ERPC_FILE_NOT_EXISTS);
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
    SendOpenError(cs->channel, uid, ERPC_GENERIC);
}

void TClientHandler::CloseFile(const acto::remote::message_t* msg, TClientSession* cs) {
    const CloseRequest*   req = (const CloseRequest*)msg->data;
    // -
    {
        acto::core::MutexLocker lock(guard);

        cs->files.erase(req->stream);
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
    SendCommonResponse(cs->channel, RPC_CLOSE, 0);
}

void TClientHandler::CloseSession(const acto::remote::message_t* msg, TClientSession* cs) {
    const ClientCloseSession*  req = (const ClientCloseSession*)msg->data;

    const ClientMap::iterator i = ctx.clients.find(req->client);

    if (i != ctx.clients.end())
        i->second->closed = true;
    // -
    printf("close session: %Zu\n", (size_t)req->client);
}

void TClientHandler::on_disconnected(void* param) {
    printf("DoClientDisconnected\n");
    TClientSession* const cs = static_cast<TClientSession*>(param);

    const ClientMap::iterator i = ctx.clients.find(cs->sid);
    if (i != ctx.clients.end())
        ctx.clients.erase(i);

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
    delete this;
}

void TClientHandler::on_message(const acto::remote::message_t* msg, void* param) {
    printf("client on_message %s\n", RpcCommandString(msg->code));
    TClientSession* const cs = static_cast<TClientSession*>(param);

    switch (msg->code) {
    case RPC_CLIENT_CONNECT: ClientConnect(msg, cs);
        break;
    case RPC_OPENFILE:       OpenFile(msg, cs);
        break;
    case RPC_CLOSE:          CloseFile(msg, cs);
        break;
    case RPC_CLOSESESSION:   CloseSession(msg, cs);
        break;
    }
};
