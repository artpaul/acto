
#include <assert.h>
#include <signal.h>
#include <stdlib.h>

#include <list>
#include <string>
#include <map>
#include <vector>

#include <port/strings.h>
#include <port/fnv.h>

#include "master.h"


#define CLIENTPORT  21581
#define CHUNKPORT   32541
#define SERVERIP    "127.0.0.1"


TMasterServer       ctx;
ui64                chunkId = 0;
ui64                client_id = 0;

/// Таблица подключенных узлов-данных
TChunkMap           chunks;

acto::core::mutex_t guard;

//------------------------------------------------------------------------------
TChunk* chunkById(const ui64 uid) {
    const TChunkMap::iterator i = chunks.find(uid);

    if (i != chunks.end())
        return i->second;
    return 0;
}


void TMasterServer::ClientConnected(acto::remote::message_channel_t* const mc, void* param) {
    printf("ChunkConnected\n");
    TClientSession* const  cs = new TClientSession();

    cs->sid     = 0;
    cs->channel = mc;
    cs->closed  = false;

    mc->set_handler(new TClientHandler, cs);
}

void TMasterServer::ChunkConnected(acto::remote::message_channel_t* const mc, void* param) {
    printf("ChunkConnected\n");
    TNodeSession* ns = new TNodeSession();
    // -
    ns->chunk = NULL;
    // -
    mc->set_handler(new TChunkHandler(), ns);
}

void TMasterServer::Run() {
    chunk_net.open (SERVERIP, CHUNKPORT, &TMasterServer::ChunkConnected, 0);
    client_net.open(SERVERIP, CLIENTPORT, &TMasterServer::ClientConnected, 0);
    so_loop(-1, 0, 0);
}

static void handleInterrupt(int sig) {
    printf("interrupted...\n");
    //so_close(ctx.clientListen);
    exit(0);
}

int main(int argc, char* argv[]) {
    // -
    //signal(SIGINT, &handleInterrupt);
    // -
    so_init();

    ctx.Run();

    so_terminate();
    return 0;
}
