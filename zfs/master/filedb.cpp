
#include <assert.h>
#include <unistd.h>

#include <port/fnv.h>

#include "filetree.h"

//-----------------------------------------------------------------------------
TFileDatabase::TFileDatabase() {
}
//-----------------------------------------------------------------------------
int TFileDatabase::OpenFile(
    const char* path,
    size_t      len,
    LockType    lock,
    NodeType    nt,
    bool        create,
    TFileNode** fn)
{
    PathParts               parts;
    cl::const_char_iterator ci(path, len);
    const fileid_t          uid = fnvhash64(path, len);
    // -
    *fn = NULL;
    // -
    if (!parsePath(ci, parts))
        return ERROR_INVALID_FILENAME;
    // -
    TFileNode* node = NULL;
    // Если необходимо создать файл, то также необходимо
    // создать промежуточные директории
    if (create) {
        if (!this->MakeupFilepath(parts, &node))
            return ERROR_INVALID_FILENAME;
        // -
        node->type  = nt;
        node->uid   = uid;
        node->locks = 0;
        node->refs++;
    }
    else {
        node = this->findPath(ci);
        if (node == NULL)
            return ERROR_FILE_NOT_EXISTS;
        assert(node->uid == uid);
        // -
        node->refs++;
    }
    mOpenedFiles[uid] = node;
    *fn = node;
    return 0;
}

//-----------------------------------------------------------------------------
bool TFileDatabase::CheckExisting(const char* path, size_t len) const {
    return this->findPath(cl::const_char_iterator(path, len)) != 0;
}
//-----------------------------------------------------------------------------
TFileNode* TFileDatabase::FileById(const fileid_t uid) const {
    TFileMap::const_iterator i = mOpenedFiles.find(uid);
    // -
    if (i != mOpenedFiles.end())
        return i->second;
    return NULL;
}

//-----------------------------------------------------------------------------
TFileNode* TFileDatabase::findPath(cl::const_char_iterator path) const {
    PathParts parts;
    // -
    if (!parsePath(path, parts))
        return 0;
    // -
    const TFileNode* node = &mRoot;
    PathParts::const_iterator j = parts.begin();
    // -
    while (node != 0 && j != parts.end()) {
        bool hasPart = false;
        for (std::list<TFileNode*>::const_iterator i = node->children.begin(); i != node->children.end(); ++i) {
            if ((*i)->name == *j) {
                node = (*i);
                hasPart = true;
                break;
            }
        }
        if (!hasPart)
            return NULL;
        ++j;
    }
    return (node != 0 && j == parts.end()) ? const_cast<TFileNode*>(node) : 0;
}
//-----------------------------------------------------------------------------
bool TFileDatabase::parsePath(cl::const_char_iterator path, PathParts& parts) const {
    cl::const_char_iterator p  = path;
    cl::const_char_iterator st = p + 1;
    // -
    if (*p != '/')
        return false;
    ++p;
    while (p.valid()) {
        while (p.valid() && *p != '/')
            p++;
        if (p > st) {
            parts.push_back(cl::string(~st, p - st));
            if (p.valid()) {
                p++;
                st = p + 1;
            }
            else
                st = p;
        }
    }
    if (p > st)
        parts.push_back(cl::string(~st, p - st));
    return true;
}

bool TFileDatabase::MakeupFilepath(const PathParts& parts, TFileNode** fn) {
    TFileNode* node = &mRoot;
    PathParts::const_iterator j = parts.begin();
    // -
    while (j != parts.end()) {
        bool hasPart = false;
        // 1. Поиск имени каталога или файла
        for (std::list<TFileNode*>::iterator i = node->children.begin(); i != node->children.end(); ++i) {
            if ((*i)->name == *j) {
                j++;
                node = (*i);
                hasPart = true;
                break;
            }
        }

        if (hasPart) {
            // Текущий компонент пути файл, а значит он не может быть
            // не конечным элементом пути
            if (node->type == ntFile) {
                *fn = node;
                return false;
            }
        }
        else {
            TFileNode* const nn = new TFileNode();

            nn->name = *j;
            nn->uid  = 0;
            nn->type = ntDirectory;

            node->children.push_back(nn);
            node = nn;
            j++;
        }
    }

    *fn = node;
    return true;
}
