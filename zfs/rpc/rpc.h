
/*
 * Remote procedure call interface
 */

#ifndef rpc_h__
#define rpc_h__

#include <system/platform.h>
#include <port/macros.h>

/* */

/// Режим чтения
const uint64_t  ZFS_READ        = (1 << 1);
/// Режим присоединения данных к концу файла
const uint64_t  ZFS_APPEND      = (1 << 2);
///
const uint64_t  ZFS_MERGE       = (1 << 3);
///
const uint64_t  ZFS_SNAPSHOT    = (1 << 4);
/// Неблокирующий доступ к файлу
const uint64_t  ZFS_SHARED      = (1 << 5);
/// Эксклюзивный доступ к файлу
const uint64_t  ZFS_EXCLUSIVE   = (1 << 6);
///
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
///
const uint16_t  RPC_CLIENT_CONNECT  = 0x0001;
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
/// Сообщение уже было обработано
#define ERPC_ALREADY_PROCESSED  0x0007


/// Идентификатор файла
typedef ui64    fileid_t;

/// Сессионный идентификатор
typedef ui64    sid_t;

#pragma pack(push, 4)

struct TMessage {
    ui32    size;       // Суммарная длинна данных в запросе (+ заголовок)
    ui16    code;       // Command code
    i16     error;      // Код ошибки
};

///////////////


struct TAppendRequest : TMessage {
    sid_t       client;     // Идентификатор клиента
    fileid_t    stream;     // Идентификатор потока
    ui32        bytes;      // Количество байт в буфере
    ui32        crc;        // Контрольная сумма для блока данных
    ui8         futher;     // Флаг продолжения передачи
};

/// Запрос закрытия файла
struct CloseRequest : TMessage {
    sid_t       client;     // Идентификатор клиента
    fileid_t    stream;
};

/// Закрытие сессии со стороны клиента
struct ClientCloseSession : TMessage {
    sid_t       client;
};

struct TReadReqest : TMessage {
    sid_t       client;     // Идентификатор клиента
    fileid_t    stream;     // Идентификатор потока
    ui64        offset;     // Смещение в файле
    ui64        bytes;      // Количество байт для чтения
};

struct TReadResponse : TMessage {
    fileid_t    stream;     // Идентификатор потока
    ui32        crc;        // Контрольная сумма для блока данных
    ui32        bytes;      // Размер блока данных
    ui8         futher;     // Флаг продолжения передачи
};

/// Запрос открытия / создания файла
struct OpenRequest : TMessage {
    sid_t       client;     // Идентификатор клиента
    uint64_t    mode;       // Режим открытия
    uint16_t    length;     // Длинна имени файла
};

/// Ответ открытия файла
struct TOpenResponse : TMessage {
    fileid_t    stream;     // Идентификатор потока
    sockaddr_in nodeip;     //
    ui16        nodeport;   //
};

/// Запрос открытия файла на chunk
struct TOpenChunkRequest : TMessage {
    sid_t       client;     // Идентификатор клиента
    fileid_t    stream;
    ui64        mode;
};

struct TOpenChunkResponse : TMessage {
    fileid_t    file;
};


/// Установка сессии с мастер-сервером
struct TMasterSession : TMessage {
    sid_t       sid;        // Сессионный идентификатор
};


///
struct TChunkConnecting : TMessage {
    ui64        uid;
    ui64        freespace;  // Оценка свободного места в узле
    sockaddr_in ip;
    i32         port;
};

///
struct ALIGNING(4) TFileTableMessage : TMessage {
    ui64        uid;        // Идентификатор узла
    ui64        count;      // Кол-во идентификаторов файлов

    // uint64_t data[count];
};

///
struct AllocateSpace : TMessage {
    sid_t       client;     // Идентификатор клиента
    fileid_t    fileid;     // Идентификатор файла
    ui64        mode;       // Режим открытия
    ui32        lease;      // Максимально допустимое время ожидание запроса клиента
};

struct AllocateResponse : TMessage {
    sid_t       client;     // Идентификатор клиента
    fileid_t    fileid;     // Идентификатор файла
    ui64        chunk;
};

#pragma pack(pop)


inline const char* rpcErrorString(const int error) {
    switch (error) {
        case ERPC_GENERIC:    return "ERPC_GENERIC";
        case ERPC_FILEEXISTS: return "ERPC_FILEEXISTS";
        case ERPC_OUTOFSPACE: return "ERPC_OUTOFSPACE";
        case ERPC_FILE_NOT_EXISTS: return "ERPC_FILE_NOT_EXISTS";
        case ERPC_FILE_BUSY:  return "ERPC_FILE_BUSY";
        case ERPC_ALREADY_PROCESSED: return "ERPC_ALREADY_PROCESSED";
    }
    return "";
}

inline const char* RpcCommandString(const int cmd) {
    switch (cmd) {
        case RPC_NONE:           return "RPC_NONE";
        case RPC_CLIENT_CONNECT: return "RPC_CLIENT_CONNECT";
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

#define DEBUG_LOG(m)    do { fprintf(stderr, "%s\n", m); } while (0)

#define LOGGING_IF(b, m) \
    if (b) \
        fprintf(stderr, "%s\n", m);

#endif // rpc_h__
