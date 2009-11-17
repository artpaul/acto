
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

struct client_handler_t;
struct file_info_t;


typedef std::map<sid_t, client_handler_t*>  client_map_t;
typedef std::map<fileid_t, file_info_t*>    file_map_t;


/// Информация о соединении с клиентом
class client_handler_t : public acto::remote::message_handler_t  {
     ///
     bool is_allowed(file_info_t* file, sid_t client);

public:
    typedef acto::remote::message_channel_t msg_channel_t;

    sid_t           m_sid;        //
    file_map_t      m_files;      // Список открытых файлов
    msg_channel_t*  m_channel;

public:
    virtual void on_connected(acto::remote::message_channel_t* const, void* param);
    virtual void on_disconnected();
    virtual void on_message(const acto::remote::message_t* msg);
};

/// Разрешения от мастер-сервера на доступ к указанным файлам
struct master_pending_t {
    sid_t       client;     //
    fileid_t    file;       //
    time_t      deadline;   //
};

/// Информация о хранящемся файле
struct file_info_t {
    fileid_t    uid;
    time_t      lease;
    FILE*       data;
    std::vector<master_pending_t*>   pendings;
};

/** */
class master_handler_t : public acto::remote::message_handler_t {
    typedef acto::remote::message_channel_t msg_channel_t;

    msg_channel_t*  channel;

public:
    virtual void on_connected(acto::remote::message_channel_t* const, void* param);
    virtual void on_disconnected();
    virtual void on_message(const acto::remote::message_t* msg);
};

extern client_map_t         clients;
extern file_map_t           files;
extern acto::core::mutex_t  guard;

#endif // chunk_h_5de09c85b176406b88ac63e7a57b9920
