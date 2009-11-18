
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
class zeusfs_t {
    /// Карта сопоставления идентификаторов файлов и записей о файлах
    typedef std::map<fileid_t, zfs_handle_t*>   file_map_t;

private:
    volatile int    connected;  //
    int             m_error;    // Код ошибки
    int             fdmaster;   // Connection to master server
    ui64            m_sid;      // Идентификатор сессии
    file_map_t      m_files;  //

    bool SendOpenToNode(sockaddr_in nodeip, int port, fileid_t stream, mode_t mode, zfs_handle_t** nc);

public:
     zeusfs_t();
    ~zeusfs_t();

public:
    /// Присоеденить данные к файлу
    int      append(zfs_handle_t* fd, const char* buf, size_t size);

    ///
    int      close(zfs_handle_t* fd);

    ///
    int      connect(const char* ip, unsigned short port);

    /// Завершить соединение с сервером
    void     disconnect();

    /// Код последней произошедшей ошибки
    int      error() const { return m_error; }

    /// Lock object
    int      lock(const char* name, mode_t mode, int wait);

    /// \brief Открыть файл
    zfs_handle_t* open(const char* name, mode_t mode);

    /// \brief Прочитать size байт из потока в буфер
    int      read(zfs_handle_t* fd, void* buf, size_t size);
};

}; // namespace zfs

#endif // ZFSLIB_H__
