
#ifndef chunk_h_5de09c85b176406b88ac63e7a57b9920
#define chunk_h_5de09c85b176406b88ac63e7a57b9920

#include <map>
#include <vector>

#include <system/mutex.h>
#include <remote/libsocket/libsocket.h>
#include <rpc/rpc.h>
#include <rpc/channel.h>

#define MASTERIP    "127.0.0.1"
#define SERVERIP    "127.0.0.1"

#define MASTERPORT  32541
#define CLIENTPORT  32543

struct TClientInfo;
struct TFileInfo;


typedef std::map<sid_t, TClientInfo*>    ClientsMap;
typedef std::map<fileid_t, TFileInfo*>   FilesMap;


/// Информация о соединении с клиентом
struct TClientInfo {
    sid_t       sid;    //
    FilesMap    files;  // Список открытых файлов
};

/// Разрешения от мастер-сервера на доступ к указанным файлам
struct MasterPending {
    sid_t       client;     //
    fileid_t    file;       //
    time_t      deadline;   //
};

/// Информация о хранящемся файле
struct TFileInfo {
    fileid_t    uid;
    time_t      lease;
    FILE*       data;
    std::vector<MasterPending*>   pendings;
};

extern ClientsMap           clients;
extern FilesMap             files;
extern acto::core::mutex_t  guard;


#endif // chunk_h_5de09c85b176406b88ac63e7a57b9920
