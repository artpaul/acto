
#include "master.h"

static void send_open_error(acto::remote::message_channel_t* mc, ui64 uid, int error) {
    TOpenResponse  rsp;
    rsp.size   = sizeof(rsp);
    rsp.code   = RPC_OPENFILE;
    rsp.error  = error;
    rsp.stream = uid;

    mc->send_message(&rsp, sizeof(rsp));
}

void chunk_handler_t::node_connect(const acto::remote::message_t* msg) {
    TChunkConnecting req = *((TChunkConnecting*)msg->data);

    acto::core::MutexLocker lock(guard);

    if (this->chunk) {
        req.size  = sizeof(req);
        req.code  = RPC_NODECONNECT;
        req.error = ERPC_ALREADY_PROCESSED;
        req.uid   = this->chunk->uid;

        msg->channel->send_message(&req, sizeof(req));
        return;
    }

    chunk_t* chunk = NULL;

    if (req.uid == 0) { // Подключение нового узла
        chunkId++;
        chunk = new chunk_t();
        chunk->uid = chunkId;
    }
    else {
        if (chunkId < req.uid)
            chunkId = req.uid;
        // -
        chunk = chunkById(req.uid);
        // -
        if (chunk == 0) {
            chunk = new chunk_t();
            chunk->uid = chunkId;
        }
        else if (chunk->available) {
            // ОШИБКА: узел с данным идентификатором уже подключён
            fprintf(stderr, "duplicated chunk uid : %i\n", (int)chunk->uid);
            msg->channel->close();
        }
    }
    assert(chunk != NULL);
    // -
    this->chunk = chunk;
    // -
    chunk->uid        = chunkId;
    chunk->channel    = msg->channel;
    chunk->available  = 1;
    chunk->ip         = req.ip;
    chunk->clientport = req.port;
    ctx.m_chunks[chunk->uid] = chunk;

    req.size  = sizeof(req);
    req.code  = RPC_NODECONNECT;
    req.error = 0;
    req.uid   = chunk->uid;
    msg->channel->send_message(&req, sizeof(req));
    // -
    printf("client id: %d\n", (int)chunk->uid);
}

void chunk_handler_t::node_allocated(const acto::remote::message_t* msg) {
    typedef std::map<sid_t, client_handler_t*>::iterator    TIterator;

    acto::core::MutexLocker lock(guard);

    const AllocateResponse* req = (const AllocateResponse*)msg->data;
    TIterator i = ctx.m_clients.find(req->client);

    if (i != ctx.m_clients.end()) {
        client_handler_t* cs    = i->second;
        chunk_t*          chunk = chunkById(req->chunk);
        file_node_t*      node  = ctx.m_tree.file_by_id(req->fileid);
        // -
        if (node && chunk) {
            node->chunks.push_back(chunk);
            // Множество открытых файлов
            cs->m_files[node->uid] = node;
            // Отправить ответ клиенту
            TOpenResponse    rsp;

            rsp.size     = sizeof(rsp);
            rsp.code     = RPC_OPENFILE;
            rsp.error    = 0;
            rsp.stream   = node->uid;
            rsp.nodeip   = chunk->ip;
            rsp.nodeport = chunk->clientport;
            cs->m_channel->send_message(&rsp, sizeof(rsp));
        }
        else
            send_open_error(cs->m_channel, req->fileid, ERPC_GENERIC);
    }
}

void chunk_handler_t::node_allow_access(const acto::remote::message_t* msg) {
    typedef std::map<sid_t, client_handler_t*>::iterator    TIterator;

    acto::core::MutexLocker lock(guard);

    const AllocateResponse* req = (const AllocateResponse*)msg->data;
    TIterator i = ctx.m_clients.find(req->client);

    if (i != ctx.m_clients.end()) {
        client_handler_t* cs    = i->second;
        chunk_t*          chunk = chunkById(req->chunk);
        file_node_t*      node  = ctx.m_tree.file_by_id(req->fileid);
        // -
        if (node && chunk) {
            // Множество открытых файлов
            cs->m_files[node->uid] = node;
            // Отправить ответ клиенту
            TOpenResponse    rsp;

            rsp.size     = sizeof(rsp);
            rsp.code     = RPC_OPENFILE;
            rsp.error    = 0;
            rsp.stream   = node->uid;
            rsp.nodeip   = chunk->ip;
            rsp.nodeport = chunk->clientport;
            cs->m_channel->send_message(&rsp, sizeof(rsp));
        }
        else
            send_open_error(cs->m_channel, req->fileid, ERPC_FILE_NOT_EXISTS);
    }
}

void chunk_handler_t::on_connected(acto::remote::message_channel_t* const mc, void* param) {
    DEBUG_LOG("ChunkConnected");
    // -
    this->chunk = NULL;
}

void chunk_handler_t::on_disconnected() {
    DEBUG_LOG("DoChunkDisconnected");
    // -
    if (this->chunk) {
        this->chunk->channel   = NULL;
        this->chunk->available = 0;
    }

    delete this;
}

void chunk_handler_t::on_message(const acto::remote::message_t* msg) {
    fprintf(stderr, "chunk on_message : %s\n", rpc_command_string(msg->code));

    switch (msg->code) {
    case RPC_ALLOCATE:    node_allocated(msg);
        break;
    case RPC_ALLOWACCESS: node_allow_access(msg);
        break;
    case RPC_NODECONNECT: node_connect(msg);
        break;
    }
}

/*
//-----------------------------------------------------------------------------
static void DoNodeFileTable(int s, const RpcHeader* const hdr, void* param) {
    TNodeSession* const ns = static_cast<TNodeSession*>(param);
    TFileTableMessage   msg;
    // -
    int rval = so_readsync(s, &msg, sizeof(msg), 5);

    if (rval > 0 && msg.count > 0) {
        cl::array_ptr<ui64> buf(new ui64[msg.count]);
        size_t              length = msg.count * sizeof(ui64);

        rval = so_readsync(s, buf.get(), length, 15);

        if (rval == length) {
            printf("file-table readed\n");
        }
    }
}
*/
