
#ifndef __master_structs_h__
#define __master_structs_h__

#include <system/platform.h>
#include <remote/transport.h>

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


// TABLE FILES : uid, parent, name, options

#endif // __master_structs_h__
