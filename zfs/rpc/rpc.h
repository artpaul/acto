
/*
 * Remote procedure call interface
 */

#ifndef rpc_h__
#define rpc_h__

#include <system/platform.h>


/* Глобальные константы */

#define DEFAULT_FILE_BLOCK      4096

#define ZFS_MAX_PATH            1024


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

const ui16  RPC_NONE            = 0x0000;

/// Открыть/создать файл
const ui16  RPC_FILE_OPEN       = 0x0001;
/// Режим чтения данных
const ui16  RPC_FILE_READ       = 0x0002;
/// Добавить данные к файлу
const ui16  RPC_FILE_APPEND     = 0x0003;
/// Закрыть файл
const ui16  RPC_FILE_CLOSE      = 0x0004;
///
const ui16  RPC_FILE_UNLINK     = 0x0005;
///
const ui16  RPC_FILE_WRITE      = 0x0006;

///
const ui16  RPC_CLIENT_CONNECT  = 0x0007;
/// Закрыть текущее соединение и завершить сессию
const ui16  RPC_CLOSESESSION    = 0x0008;


/* Сообщения инициируемые мастер-сервером */

/// Выделить область под хранения данных
const ui16  RPC_ALLOCATE        = 0x0101;
/// Разрешить доступ к определенному файлу
const ui16  RPC_ALLOWACCESS     = 0x0102;


/* Сообщения инициируемые узлом данных */

/// Установка соединения с мастер-сервером
const ui16  RPC_NODECONNECT     = 0x0501;
/// Передать список файлов, хранящихся на узле
const ui16  RPC_NODE_FILETABLE  = 0x0502;


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

#define ERPC_FILE_GENERIC       0x0008



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

typedef TMessage    rpc_message_t;

///////////////


struct TAppendRequest : TMessage {
    sid_t       client;     // Идентификатор клиента
    fileid_t    stream;     // Идентификатор потока
    ui32        bytes;      // Количество байт в буфере
    ui32        crc;        // Контрольная сумма для блока данных
    ui8         futher;     // Флаг продолжения передачи
};

/// Запрос закрытия файла
struct rpc_close_file_t : rpc_message_t {
    sid_t       client;     // Идентификатор клиента
    ui64        cid;        //
    ui64        uid;        //
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
struct rpc_open_request_t : rpc_message_t {
    sid_t       client;     // Идентификатор клиента
    ui64        mode;       // Режим открытия
    ui32        length;     // Длинна имени файла
    char        path[ZFS_MAX_PATH];
};

/// Ответ открытия файла
struct rpc_open_response_t : rpc_message_t {
    ui64        cid;        // Идентификатор в каталоге
    ui64        uid;        // Идентификатор физического файла
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
struct TFileTableMessage : TMessage {
    ui64        uid;        // Идентификатор узла
    ui64        count;      // Кол-во идентификаторов файлов

    // uint64_t data[count];
};

///
struct rpc_allocate_space_t : rpc_message_t {
    sid_t       client;     // Идентификатор клиента
    ui64        cid;        //
    ui64        uid;        // Идентификатор файла
    ui64        mode;       // Режим открытия
    ui32        lease;      // Максимально допустимое время ожидание запроса клиента
};

struct rpc_allocate_response_t : rpc_message_t {
    sid_t       client;     // Идентификатор клиента
    ui64        cid;        //
    ui64        uid;        // Идентификатор файла
    ui64        chunk;
};

struct rpc_file_unlink_t : rpc_message_t {
    sid_t       client;
    ui32        length;
    char        path[ZFS_MAX_PATH];
};

#pragma pack(pop)


inline const char* rpc_error_string(const int error) {
    switch (error) {
        case ERPC_GENERIC:           return "ERPC_GENERIC";
        case ERPC_FILEEXISTS:        return "ERPC_FILEEXISTS";
        case ERPC_OUTOFSPACE:        return "ERPC_OUTOFSPACE";
        case ERPC_FILE_NOT_EXISTS:   return "ERPC_FILE_NOT_EXISTS";
        case ERPC_FILE_BUSY:         return "ERPC_FILE_BUSY";
        case ERPC_ALREADY_PROCESSED: return "ERPC_ALREADY_PROCESSED";
        case ERPC_FILE_GENERIC:      return "ERPC_FILE_GENERIC";
    }
    return "";
}

inline const char* rpc_command_string(const int cmd) {
    switch (cmd) {
        case RPC_NONE:           return "RPC_NONE";
        case RPC_CLIENT_CONNECT: return "RPC_CLIENT_CONNECT";
        case RPC_FILE_OPEN:      return "RPC_FILE_OPEN";
        case RPC_FILE_READ:      return "RPC_FILE_READ";
        case RPC_FILE_APPEND:    return "RPC_FILE_APPEND";
        case RPC_FILE_CLOSE:     return "RPC_FILE_CLOSE";
        case RPC_FILE_UNLINK:    return "RPC_FILE_UNLINK";
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
