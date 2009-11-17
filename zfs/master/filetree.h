
#ifndef __master_filetree_h__
#define __master_filetree_h__

#include <arpa/inet.h>

#include <list>
#include <map>
#include <vector>

#include <system/platform.h>
#include <port/strings.h>

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


/** Метаинформация о файле */
struct file_node_t {
/* Структура дерева */
    file_node_t*            parent;
    cl::string              name;      //
    std::list<file_node_t*> children;  //
    NodeType                type;

    fileid_t                uid;        // Идентификатор файла
    std::vector<chunk_t*>   chunks;     // Узлы, на которых расположены данные этого файла
    ui8                     state;

/* Состояние открытого файла */
    i32                     refs;       // Счетчик ссылок на файл
    i32                     locks;      //
};


///
const int ERROR_INVALID_FILENAME = 0x0001;
const int ERROR_FILE_NOT_EXISTS  = 0x0002;


/**
 * Дерево файловой системы
 */
class file_database_t {
    typedef std::map<fileid_t, file_node_t*>    file_map_t;
    typedef std::list<cl::string>               path_parts_t;

private:
    file_node_t     mRoot;
    /// Таблица открытых файлов
    file_map_t      mOpenedFiles;

    file_node_t*  find_path(cl::const_char_iterator path) const;
    bool parse_path(cl::const_char_iterator path, path_parts_t& parts) const;
    bool makeup_filepath(const path_parts_t& parts, file_node_t** fn);

public:
    file_database_t();

    ///
    int close_file(fileid_t uid);

    /// Открыть существующий файл или создать новый, если create == true
    int open_file (const char* path, size_t len, LockType lock, NodeType nt, bool create, file_node_t** fn);

    ///
    bool         check_existing(const char* path, size_t len) const;
    ///
    file_node_t* file_by_id(const fileid_t uid) const;
};

#endif // __master_filetree_h__
