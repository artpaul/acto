
#ifndef chunk_h_5de09c85b176406b88ac63e7a57b9920
#define chunk_h_5de09c85b176406b88ac63e7a57b9920

#include <memory.h>

#include <map>
#include <vector>

#include <system/mutex.h>
#include <remote/transport.h>
#include <rpc/rpc.h>

#define MASTERIP    "127.0.0.1"
#define SERVERIP    "127.0.0.1"

#define MASTERPORT  32541
#define CLIENTPORT  32543

struct TClientHandler;
struct TFileInfo;


typedef std::map<sid_t, TClientHandler*> ClientsMap;
typedef std::map<fileid_t, TFileInfo*>   FilesMap;


/// Информация о соединении с клиентом
class TClientHandler : public acto::remote::message_handler_t  {
public:
    typedef acto::remote::message_channel_t msg_channel_t;

    sid_t           sid;        //
    FilesMap        files;      // Список открытых файлов
    msg_channel_t*  channel;

public:
    virtual void on_connected(acto::remote::message_channel_t* const, void* param);
    virtual void on_disconnected();
    virtual void on_message(const acto::remote::message_t* msg);
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

/** */
class TMasterHandler : public acto::remote::message_handler_t {
    typedef acto::remote::message_channel_t msg_channel_t;

    msg_channel_t*  channel;
public:
    virtual void on_connected(acto::remote::message_channel_t* const, void* param);
    virtual void on_disconnected();
    virtual void on_message(const acto::remote::message_t* msg);
};

extern ClientsMap           clients;
extern FilesMap             files;
extern acto::core::mutex_t  guard;

#endif // chunk_h_5de09c85b176406b88ac63e7a57b9920
