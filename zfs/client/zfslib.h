
// Client library for Z-file-system

#ifndef ZFSLIB_H__
#define ZFSLIB_H__

#include <stdint.h>
#include <arpa/inet.h>

#include <map>

#include <rpc/rpc.h>

namespace zfs {

const int   EZFS_CONNECTION = 0x0001;


/// Режим доступа к файлам
typedef uint64_t    mode_t;

/// -
struct zfs_handle_t;


/// Сессия соединения с кластером файловой системы
class TZeusFS {
    /// Карта сопоставления идентификаторов файлов и записей о файлах
    typedef std::map<fileid_t, zfs_handle_t*>     TStreamMap;

private:
    volatile int    connected;  //
    int             m_error;    // Код ошибки
    int             fdmaster;   // Connection to master server
    __int64_t       m_sid;      // Идентификатор сессии
    TStreamMap      m_streams;  //

    bool SendOpenToNode(sockaddr_in nodeip, int port, fileid_t stream, mode_t mode, zfs_handle_t** nc);

public:
     TZeusFS();
    ~TZeusFS();

public:
    /// Присоеденить данные к файлу
    int      Append(zfs_handle_t* fd, const char* buf, size_t size);
    ///
    int      Close(zfs_handle_t* fd);
    ///
    int      connect(const char* ip, unsigned short port);
    /// Завершить соединение с сервером
    void     disconnect();
    /// Код последней произошедшей ошибки
    int      error() const { return m_error; }
    /// Lock object
    int      lock(const char* name, mode_t mode, int wait);
    /// \brief Открыть файл
    zfs_handle_t* Open(const char* name, mode_t mode);
    /// \brief Прочитать size байт из потока в буфер
    int      Read(zfs_handle_t* fd, void* buf, size_t size);
};

}; // namespace zfs

#endif // ZFSLIB_H__
