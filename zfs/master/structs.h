
#ifndef __master_structs_h__
#define __master_structs_h__

#include <system/platform.h>

#include "filetree.h"

/*
#include <uuid/uuid.h>

void uuid_generate(uuid_t out);
void uuid_generate_random(uuid_t out);
void uuid_generate_time(uuid_t out);
*/

///////////////////////////////////////////////////////////////////////////////
// БАЗА МЕТАДАННЫХ ФАЙЛОВОЙ СИСТЕМЫ

const ui8 FT_REGULAR   = 0x01;
const ui8 FT_DIRECTORY = 0x02;

/** */
struct TFileRecord {
    ui64        uid;            // Идентификатор файла
    ui64        parent;         // Идентификатор каталога в котором находится файл
    ui8         type;           // Тип файла
    char        path[1024];     // Полный путь к файлу
};

/** */
struct TLocationRecord {
    ui64        file;           // Идентификатор файла
};

///////////////////////////////////////////////////////////////////////////////



/** Параметры сервера-данных */
struct TChunk {
    ui64            uid;            //
    sockaddr_in     ip;             // Node ip
    TChunk*         slave;          //
    int             s;              //
    int             available : 1;  //
};

///
struct TNodeSession {
    TChunk*             chunk;  //
    int                 s;      // Socket
    struct sockaddr_in  addr;   //
};

/** Состояние соединения с клиентским приложением */
struct TClientSession {
    typedef std::map<fileid_t, TFileNode*>   TFiles;

    sid_t           sid;        // Уникальный идентификатор сессии
    sockaddr_in     addr;       //
    bool            closed;     // Флаг штатного закрытия сессии
    // Список открытых/заблокированных файлов
    TFiles          files;
};

/// Server data context
struct TContext {
    TFileDatabase   tree;

    int             chunkListen;    // Chunk socket
    int             clientListen;   // Client socket
};

typedef std::map<ui64, TChunk*>             TChunkMap;
typedef std::map<sid_t, TClientSession*>    ClientMap;

// TABLE FILES : uid, parent, name, options

#endif // __master_structs_h__
