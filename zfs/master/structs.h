
#ifndef __master_structs_h__
#define __master_structs_h__

#include "filetree.h"

/*
#include <uuid/uuid.h>

void uuid_generate(uuid_t out);
void uuid_generate_random(uuid_t out);
void uuid_generate_time(uuid_t out);
*/

/** Параметры сервера-данных */
struct TChunk {
    __uint64_t  uid;            //
    sockaddr_in ip;             // Node ip
    TChunk*     slave;          //
    int         s;              //
    int         available : 1;  //
};

///
struct TNodeSession {
    TChunk*             chunk;  //
    int                 s;      // Socket
    struct sockaddr_in  addr;   //
};

/** Состояние соединения с клиентским приложением */
struct TClientSession {
    typedef std::map<fileid_t, FileInfo*>   TFiles;

    sid_t       sid;        // Уникальный идентификатор сессии
    sockaddr_in addr;       //
    bool        closed;     // Флаг штатного закрытия сессии
    // Список открытых/заблокированных файлов
    TFiles      files;
};

/// Server data context
struct TContext {
    TFileTree   tree;

    int         chunkListen;    // Chunk socket
    int         clientListen;   // Client socket
};


/** Состояние открытого файла */
struct TFileContext {
    long        refs;       // Счетчик ссылок на файл
    int         locks;      //
};

/** Метаинформация о файле */
struct FileInfo {
    fileid_t                uid;        // Идентификатор файла
    TFileContext*           openctx;    // Контекст досупа к файлу
    std::vector<uint64_t>   chunks;     // Узлы, на которых расположены данные этого файла
};


typedef std::map<__uint64_t, TChunk*>       ChunkMap;
typedef std::map<sid_t, TClientSession*>    ClientMap;
typedef std::map<fileid_t, FileInfo*>       TFileMap;

// TABLE FILES : uid, parent, name, options

#endif // __master_structs_h__
