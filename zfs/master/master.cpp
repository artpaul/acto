
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


master_server_t     ctx;
ui64                chunkId = 0;
ui64                client_id = 0;

acto::core::mutex_t guard;

//------------------------------------------------------------------------------
chunk_t* chunkById(const ui64 uid) {
    const chunk_map_t::iterator i = ctx.m_chunks.find(uid);

    if (i != ctx.m_chunks.end())
        return i->second;
    return 0;
}

void master_server_t::run() {
    chunk_net.open (SERVERIP, CHUNKPORT,  0);
    client_net.open(SERVERIP, CLIENTPORT, 0);
    so_loop(-1, 0, 0);
}

static void handleInterrupt(int sig) {
    fprintf(stderr, "interrupted...\n");
    //so_close(ctx.clientListen);
    exit(0);
}

int main(int argc, char* argv[]) {
    // -
    //signal(SIGINT, &handleInterrupt);
    // -
    so_init();

    ctx.run();

    so_terminate();
    return 0;
}
