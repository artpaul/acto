
/*
 * Remote procedure call interface
 */

#ifndef rpc_h__
#define rpc_h__

#include <port/macros.h>

/* */

/// Режим чтения
const uint64_t  ZFS_READ        = (1 << 1);
/// Режим присоединения данных к концу файла
const uint64_t  ZFS_APPEND      = (1 << 2);
///
const uint64_t  ZFS_MERGE       = (1 << 3);
const uint64_t  ZFS_SNAPSHOT    = (1 << 4);
/// Неблокирующий доступ к файлу
const uint64_t  ZFS_SHARED      = (1 << 5);
/// Эксклюзивный доступ к файлу
const uint64_t  ZFS_EXCLUSIVE   = (1 << 6);
const uint64_t  ZFS_WRITE       = (1 << 7);
/// Создать объект если не существует
const uint64_t  ZFS_CREATE      = (1 << 8);
/// Флаг создания директории
const uint64_t  ZFS_DIRECTORY   = (1 << 9);

/*
#define ZFF_LOCKEXCLUSIVE   8
#define ZFF_LOCKSNAPSHOT    9
#define ZFF_LOCKREAD        10
#define ZFF_LOCKAPPEN       11
#define ZFF_LOCKMERGE       12
#define ZFF_LOCKUNLOCK      14
#define ZFF_LOCKWAIT        15
*/


/* Сообщения инициируемые клиентом */

const uint16_t  RPC_NONE            = 0x0000;
/// Открыть/создать файл
const uint16_t  RPC_OPENFILE        = 0x0002;
/// Режим чтения данных
const uint16_t  RPC_READ            = 0x0003;
/// Добавить данные к файлу
const uint16_t  RPC_APPEND          = 0x0004;
/// Закрыть файл
const uint16_t  RPC_CLOSE           = 0x0005;
/// Закрыть текущее соединение и завершить сессию
const uint16_t  RPC_CLOSESESSION    = 0x0009;


/* Сообщения инициируемые мастер-сервером */

/// Выделить область под хранения данных
const uint16_t  RPC_ALLOCATE        = 0x0101;
/// Разрешить доступ к определенному файлу
const uint16_t  RPC_ALLOWACCESS     = 0x0102;


/* Сообщения инициируемые узлом данных */

/// Установка соединения с мастер-сервером
const uint16_t  RPC_NODECONNECT     = 0x0501;
/// Передать список файлов, хранящихся на узле
const uint16_t  RPC_NODE_FILETABLE  = 0x0502;


///
#define ERPC_GENERIC            0x0001
/// Файл существует
#define ERPC_FILEEXISTS         0x0002
/// Нет свободного места для записи данных
#define ERPC_OUTOFSPACE         0x0003
///
#define ERPC_BADMODE            0x0004

#define ERPC_FILE_NOT_EXISTS    0x0005

#define ERPC_FILE_BUSY          0x0006


#pragma pack(push, 4)

struct ALIGNING(4) RpcHeader {
    uint32_t    size;       // Суммарная длинна данных в запросе (+ заголовок)
    uint16_t    code;       // Command code
    uint8_t     dummy[2];   //
};

///////////////

/// Идентификатор файла
typedef __int64_t      fileid_t;

/// Сессионный идентификатор
typedef __int64_t      sid_t;

/// Идентификатор файла
//typedef __int128_t      fileid_t;
/// Сессионный идентификатор
//typedef __int128_t      sid_t;


///
struct ALIGNING(4) TCommonResponse : public RpcHeader {
    int err;
};

struct ALIGNING(4) AppendRequest : public RpcHeader {
    sid_t       client;     // Идентификатор клиента
    fileid_t    stream;     // Идентификатор потока
    uint32_t    bytes;      // Количество байт в буфере
    uint32_t    crc;        // Контрольная сумма для блока данных
    bool        futher;     // Флаг продолжения передачи
};

/// Запрос закрытия файла
struct ALIGNING(4) CloseRequest : public RpcHeader {
    sid_t       client;     // Идентификатор клиента
    fileid_t    stream;
};

/// Закрытие сессии со стороны клиента
struct ALIGNING(4) ClientCloseSession : public RpcHeader {
    sid_t       client;
};

struct ALIGNING(4) TReadReqest : public RpcHeader {
    sid_t       client;     // Идентификатор клиента
    fileid_t    stream;     // Идентификатор потока
    uint64_t    offset;     // Смещение в файле
    uint64_t    bytes;      // Количество байт для чтения
};

struct ALIGNING(4) ReadResponse {
    fileid_t    stream;     // Идентификатор потока
    uint32_t    crc;        // Контрольная сумма для блока данных
    uint32_t    size;       // Размер блока данных
    bool        futher;     // Флаг продолжения передачи
};

/// Запрос открытия / создания файла
struct ALIGNING(4) OpenRequest : public RpcHeader {
    sid_t       client;     // Идентификатор клиента
    uint64_t    mode;       // Режим открытия
    uint16_t    length;     // Длинна имени файла
};

/// Ответ открытия файла
struct ALIGNING(4) OpenResponse {
    fileid_t    stream;     // Идентификатор потока
    uint32_t    err;        // Код ошибки
    sockaddr_in nodeip;     //
};

/// Запрос открытия файла на chunk
struct ALIGNING(4)  OpenChunkRequest : public RpcHeader {
    sid_t       client;     // Идентификатор клиента
    fileid_t    stream;
    uint64_t    mode;
};

struct ALIGNING(4)  OpenChunkResponse {
    fileid_t    file;
    uint32_t    err;
};


/// Установка сессии с мастер-сервером
struct ALIGNING(4) MasterSession {
    sid_t       sid;        // Сессионный идентификатор
};


///
struct ALIGNING(4) ChunkConnecting : public RpcHeader {
    __uint64_t  uid;
    uint64_t    freespace;  // Оценка свободного места в узле
};

///
struct ALIGNING(4) TFileTableMessage : public RpcHeader {
    __uint64_t      uid;        // Идентификатор узла
    __uint64_t      count;      // Кол-во идентификаторов файлов

    // uint64_t data[count];
};

///
struct ALIGNING(4) AllocateSpace : public RpcHeader {
    sid_t       client;     // Идентификатор клиента
    fileid_t    fileid;     // Идентификатор файла
    uint64_t    mode;       // Режим открытия
    uint32_t    lease;      // Максимально допустимое время ожидание запроса клиента
};

struct ALIGNING(4) AllocateResponse {
    uint16_t    err;
};

#pragma pack(pop)


inline const char* rpcErrorString(const int error) {
    switch (error) {
        case ERPC_GENERIC:    return "ERPC_GENERIC";
        case ERPC_FILEEXISTS: return "ERPC_FILEEXISTS";
        case ERPC_OUTOFSPACE: return "ERPC_OUTOFSPACE";
        case ERPC_FILE_NOT_EXISTS: return "ERPC_FILE_NOT_EXISTS";
        case ERPC_FILE_BUSY:  return "ERPC_FILE_BUSY";
    }
    return "";
}

inline const char* RpcCommandString(const int cmd) {
    switch (cmd) {
        case RPC_NONE:           return "RPC_NONE";
        case RPC_OPENFILE:       return "RPC_OPENFILE";
        case RPC_READ:           return "RPC_READ";
        case RPC_APPEND:         return "RPC_APPEND";
        case RPC_CLOSE:          return "RPC_CLOSE";
        case RPC_CLOSESESSION:   return "RPC_CLOSESESSION";
        case RPC_ALLOCATE:       return "RPC_ALLOCATE";
        case RPC_ALLOWACCESS:    return "RPC_ALLOWACCESS";
        case RPC_NODECONNECT:    return "RPC_NODECONNECT";
        case RPC_NODE_FILETABLE: return "RPC_NODE_FILETABLE";
    }
    return "";
};

#endif // rpc_h__
