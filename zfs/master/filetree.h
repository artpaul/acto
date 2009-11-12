
#ifndef __master_filetree_h__
#define __master_filetree_h__

#include <list>
#include <vector>

#include <port/strings.h>

struct TChunk;

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
struct TFileNode {
/* Структура дерева */
    TFileNode*              parent;
    cl::string              name;      //
    std::list<TFileNode*>   children;  //
    NodeType                type;

    fileid_t                uid;        // Идентификатор файла
    std::vector<TChunk*>    chunks;     // Узлы, на которых расположены данные этого файла
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
class TFileDatabase {
    typedef std::map<fileid_t, TFileNode*>  TFileMap;
    typedef std::list<cl::string>           PathParts;

private:
    TFileNode   mRoot;
    /// Таблица открытых файлов
    TFileMap    mOpenedFiles;

    TFileNode*  findPath(cl::const_char_iterator path) const;
    bool parsePath(cl::const_char_iterator path, PathParts& parts) const;
    bool MakeupFilepath(const PathParts& parts, TFileNode** fn);

public:
    TFileDatabase();

    ///
    int CloseFile (fileid_t uid);
    /// Открыть существующий файл или создать новый, если create == true
    int OpenFile  (const char* path, size_t len, LockType lock, NodeType nt, bool create, TFileNode** fn);

    ///
    bool        CheckExisting(const char* path, size_t len) const;
    ///
    TFileNode*  FileById(const fileid_t uid) const;
};

#endif // __master_filetree_h__
