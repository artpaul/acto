
#ifndef __master_filetree_h__
#define __master_filetree_h__

#include <arpa/inet.h>

#include <list>
#include <map>
#include <string>
#include <vector>

#include <system/platform.h>
#include <port/fnv.h>

#include <rpc/rpc.h>

struct chunk_t;

enum NodeType {
    ntDirectory,
    ntFile
};

enum LockType {
    LOCK_NONE,
    LOCK_READ,
    LOCK_WRITE
};

///
const ui8 FSTATE_NONE    = 0x00;
///
const ui8 FSTATE_NEW     = 0x01;
///
const ui8 FSTATE_OPENED  = 0x02;
/// Файл существует
const ui8 FSTATE_CLOSED  = 0x03;
/// Файл удалён
const ui8 FSTATE_DELETED = 0x04;


///
#define ERROR_INVALID_FILENAME  0x0001
///
#define ERROR_FILE_NOT_EXISTS   0x0002
///
#define ERROR_FILE_EXISTS       0x0003
///
#define ERROR_FILE_LOCKED       0x0004


/**
 * Метаинформация о файле
 */
struct file_node_t {
    ui64                    uid;        // Идентификатор файла
    ui32                    count;      // Количество ссылок на файл
    ui32                    locks;      //
    file_node_t*            parent;
    std::vector<chunk_t*>   chunks;     // Узлы, на которых расположены данные этого файла
    NodeType                type;
    ui8                     state;
};

/** */
struct file_path_t {
    file_node_t*        node;
    ui64                cid;        // Идентификатор имени
    std::string         name;
};


/**
 * Дерево файловой системы
 */
class file_database_t {
    typedef std::map< ui64, file_node_t* >    file_map_t;
    typedef std::map< ui64, file_path_t* >    file_catalog_t;
    // cid -> uid

private:
    ///
    ui64            m_node_counter;

    /// Таблица каталога файловой системы
    file_catalog_t  m_catalog;
    /// Таблица открытых файлов
    file_catalog_t  m_opened;
    /// Таблица удалённых узлов
    file_map_t      m_deleted;
    /// Таблица существующих узлов
    file_map_t      m_nodes;

    bool check_path(const char* path, size_t len) const;
    ///
    void release_node(file_node_t* node);

public:
    file_database_t();

    static ui64 path_hash(const char* path, size_t len) {
        return fnvhash64(path, len);
    }

public:
    ///
    int close_file(ui64 cid);

    /// Открыть существующий файл или создать новый, если create == true
    int open_file (const char* path, size_t len, LockType lock, NodeType nt, bool create, file_path_t** fn);

    ///
    bool         check_existing(const char* path, size_t len) const;
    ///
    file_path_t* file_by_cid(ui64 cid) const;
    ///
    file_node_t* file_by_id(const fileid_t uid) const;
    ///
    int          file_unlink(const char* path, size_t len);
};

#endif // __master_filetree_h__
