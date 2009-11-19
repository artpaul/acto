
#include <port/fnv.h>

#include "master.h"

static chunk_t* choose_chunk_for_file() {
    for (chunk_map_t::iterator i = ctx.m_chunks.begin(); i != ctx.m_chunks.end(); ++i) {
        chunk_t* const chunk = i->second;
        // -
        if (chunk->channel)
            return chunk;
    }
    return 0;
}


void client_handler_t::send_common_response(acto::remote::message_channel_t* mc, ui16 cmd, i16 err) {
    TMessage resp;
    resp.size  = sizeof(resp);
    resp.code  = cmd;
    resp.error = err;

    mc->send_message(&resp, sizeof(resp));
}


void client_handler_t::send_open_error(acto::remote::message_channel_t* mc, ui64 uid, int error) {
    TOpenResponse  rsp;
    rsp.size   = sizeof(rsp);
    rsp.code   = RPC_FILE_OPEN;
    rsp.error  = error;
    rsp.stream = uid;

    mc->send_message(&rsp, sizeof(rsp));
}


void client_handler_t::client_connect(const acto::remote::message_t* msg) {
    const TMasterSession*   req = (const TMasterSession*)msg->data;
    TMasterSession          rsp = *req;
    // -
    // -
    if (req->sid == 0)
        m_sid = rsp.sid = ++client_id;
    // -
    ctx.m_clients[m_sid] = this;
    // -
    m_channel->send_message(&rsp, sizeof(rsp));
}


void client_handler_t::file_unlink(const acto::remote::message_t* msg) {
    const rpc_file_unlink_t* req = (const rpc_file_unlink_t*)msg->data;
    // -
    const fileid_t uid = file_database_t::path_hash(req->path, req->length);

    // Открыт ли файл в текущей сессии? - оставлять открытым
    int rval = ctx.m_tree.file_unlink(uid);

    rpc_message_t   rsp;
    rsp.code  = req->code;
    rsp.size  = sizeof(rsp);
    rsp.error = !rval ? 0 : ERPC_GENERIC;
    m_channel->send_message(&rsp, sizeof(rsp));
}


void client_handler_t::open_file(const acto::remote::message_t* msg) {
    const OpenRequest*  req = (const OpenRequest*)msg->data;
    const char*         name = (const char*)msg->data + sizeof(OpenRequest);
    //
    const fileid_t uid  = file_database_t::path_hash(name, req->length);
    file_node_t*   node = NULL;
    int            ferror = 0;

    if (m_files.find(uid) != m_files.end()) {
        ferror = ERPC_FILE_BUSY;
        goto send_error;
    }

    // -
    if (req->mode & ZFS_CREATE) {
        // Ошибка, если файл создаётся, но не указан режим записи
        if ((req->mode & ZFS_WRITE) == 0 && (req->mode & ZFS_APPEND) == 0) {
            ferror = ERPC_BADMODE;
            goto send_error;
        }
        // -
        int error = ctx.m_tree.open_file(name, req->length, LOCK_WRITE, (req->mode & ZFS_DIRECTORY) ? ntDirectory : ntFile, true, &node);

        if (error != 0) {
            ferror = ERPC_FILEEXISTS;
            goto send_error;
        }

        chunk_t* const chunk = choose_chunk_for_file();

        LOGGING_IF(!chunk, "chunk is NULL");

        // Выбрать узел для хранения данных
        if (!chunk || !chunk->channel)
            send_open_error(m_channel, uid, ERPC_OUTOFSPACE);
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
        int error = ctx.m_tree.open_file(name, req->length, LOCK_READ, ntFile, false, &node);

        if (error != 0) {
            send_open_error(m_channel, uid, ERPC_FILE_NOT_EXISTS);
            return;
        }
        else {
            assert(node != 0);
            // Получить карту расположения файла по секторам
            // Послать узлам команду открытия файла
            // Отправить карту клиенту
            // -
            for (size_t i = 0; i < node->chunks.size(); ++i) {
                if (!node->chunks[i]->channel)
                    continue;

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

    ferror = ERPC_GENERIC;

send_error:
    send_open_error(m_channel, uid, ferror);
}

void client_handler_t::close_file(const acto::remote::message_t* msg) {
    const CloseRequest* req = (const CloseRequest*)msg->data;
    // -
    {
        acto::core::MutexLocker lock(guard);

        m_files.erase(req->stream);
        // !!! Отметить файл как закрытый
        int rval = ctx.m_tree.close_file(req->stream);
        if (rval != 0)
            send_common_response(m_channel, RPC_FILE_CLOSE, ERPC_FILE_GENERIC);
        else
            send_common_response(m_channel, RPC_FILE_CLOSE, 0);
    }
}

void client_handler_t::close_session(const acto::remote::message_t* msg) {
    const ClientCloseSession*  req = (const ClientCloseSession*)msg->data;

    const client_map_t::iterator i = ctx.m_clients.find(req->client);

    if (i != ctx.m_clients.end())
        i->second->m_closed = true;
    // -
    fprintf(stderr, "close session: %Zu\n", (size_t)req->client);
}

void client_handler_t::on_connected(acto::remote::message_channel_t* const mc, void* param) {
    DEBUG_LOG("ClientConnected");

    m_sid     = 0;
    m_channel = mc;
    m_closed  = false;
}

void client_handler_t::on_disconnected() {
    DEBUG_LOG("DoClientDisconnected");

    const client_map_t::iterator i = ctx.m_clients.find(m_sid);
    if (i != ctx.m_clients.end())
        ctx.m_clients.erase(i);

    // Закрыть все открытые файлы
    if (!m_files.empty()) {
        //for (TClientSession::TFiles::iterator i = cs->files.begin(); i != cs->files.end(); ++i)
            //CloseFile(i->second, internal = true);
    }
    //
    if (m_closed) {
        // -
    }
    else {
        // Соединение с клиентом было потеряно.
        // Возможно это временная ошибка.
        // Необходимо подождать некоторое время восстановаления сессии

    }
}

void client_handler_t::on_message(const acto::remote::message_t* msg) {
    fprintf(stderr, "client on_message %s\n", rpc_command_string(msg->code));

    switch (msg->code) {
    case RPC_CLIENT_CONNECT: client_connect(msg);
        break;
    case RPC_FILE_OPEN:      open_file(msg);
        break;
    case RPC_FILE_CLOSE:     close_file(msg);
        break;
    case RPC_FILE_UNLINK:    file_unlink(msg);
        break;
    case RPC_CLOSESESSION:   close_session(msg);
        break;
    }
};
