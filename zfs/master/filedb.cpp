
#include <assert.h>
#include <unistd.h>

#include <port/fnv.h>

#include "filetree.h"

//-----------------------------------------------------------------------------
file_database_t::file_database_t() {
}
//-----------------------------------------------------------------------------
int file_database_t::open_file(
    const char*   path,
    size_t        len,
    LockType      lock,
    NodeType      nt,
    bool          create,
    file_node_t** fn)
{
    path_parts_t            parts;
    cl::const_char_iterator ci(path, len);
    const fileid_t          uid = fnvhash64(path, len);
    // -
    *fn = NULL;
    // -
    if (!parse_path(ci, parts))
        return ERROR_INVALID_FILENAME;
    // -
    file_node_t* node = NULL;
    // Если необходимо создать файл, то также необходимо
    // создать промежуточные директории
    if (create) {
        if (!this->makeup_filepath(parts, &node))
            return ERROR_INVALID_FILENAME;
        // -
        node->type  = nt;
        node->uid   = uid;
        node->locks = 0;
        node->refs++;
    }
    else {
        node = this->find_path(ci);
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
bool file_database_t::check_existing(const char* path, size_t len) const {
    return this->find_path(cl::const_char_iterator(path, len)) != 0;
}
//-----------------------------------------------------------------------------
file_node_t* file_database_t::file_by_id(const fileid_t uid) const {
    file_map_t::const_iterator i = mOpenedFiles.find(uid);
    // -
    if (i != mOpenedFiles.end())
        return i->second;
    return NULL;
}

//-----------------------------------------------------------------------------
file_node_t* file_database_t::find_path(cl::const_char_iterator path) const {
    path_parts_t parts;
    // -
    if (!parse_path(path, parts))
        return 0;
    // -
    const file_node_t* node = &mRoot;
    path_parts_t::const_iterator j = parts.begin();
    // -
    while (node != 0 && j != parts.end()) {
        bool hasPart = false;
        for (std::list<file_node_t*>::const_iterator i = node->children.begin(); i != node->children.end(); ++i) {
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
    return (node != 0 && j == parts.end()) ? const_cast<file_node_t*>(node) : 0;
}
//-----------------------------------------------------------------------------
bool file_database_t::parse_path(cl::const_char_iterator path, path_parts_t& parts) const {
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

bool file_database_t::makeup_filepath(const path_parts_t& parts, file_node_t** fn) {
    file_node_t* node = &mRoot;
    path_parts_t::const_iterator j = parts.begin();
    // -
    while (j != parts.end()) {
        bool hasPart = false;
        // 1. Поиск имени каталога или файла
        for (std::list<file_node_t*>::iterator i = node->children.begin(); i != node->children.end(); ++i) {
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
            file_node_t* const nn = new file_node_t();

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
