
#include <assert.h>
#include <unistd.h>
#include <stdio.h>

#include <port/fnv.h>

#include "filetree.h"

//-----------------------------------------------------------------------------
file_database_t::file_database_t()
    : m_node_counter(1)
{
}

bool file_database_t::check_path(const char* path, size_t len) const {
    return true;
}

void file_database_t::release_node(file_node_t* node) {
    assert(node != 0);
    assert(node->count > 0);

    const ui32 result = --node->count;

    if (result == 0) {
        DEBUG_LOG("file removed");
        node->state = FSTATE_DELETED;

        m_nodes.erase(node->uid);
        // -
        m_deleted[node->uid] = node;
    }
}

//-----------------------------------------------------------------------------
int file_database_t::close_file(ui64 cid) {
    file_catalog_t::const_iterator i = m_opened.find(cid);
    // -
    if (i != m_opened.end()) {
        file_node_t* const node = i->second->node;
        // -
        m_opened.erase(cid);
        // -
        node->state = FSTATE_CLOSED;
        this->release_node(node);
        return 0;
    }
    return -1;
}

//-----------------------------------------------------------------------------
int file_database_t::open_file(
    const char*   path,
    size_t        len,
    LockType      lock,
    NodeType      nt,
    bool          create,
    file_path_t** fn)
{
    *fn = NULL;

    if (!check_path(path, len))
        return ERROR_INVALID_FILENAME;

    const fileid_t cid = file_database_t::path_hash(path, len);

    const file_catalog_t::iterator i = m_catalog.find(cid);

    file_node_t* node = NULL;

    // Если необходимо создать файл, то также необходимо
    // создать промежуточные директории
    if (create) {
        file_path_t* ppath;

        if (i != m_catalog.end())
            return ERROR_FILE_EXISTS;

        node  = new file_node_t();
        ppath = new file_path_t();
        // -
        ppath->node = node;
        ppath->name = std::string(path, len);
        ppath->cid  = cid;
        // -
        node->parent = 0;
        node->type   = nt;
        node->uid    = ++m_node_counter;
        node->locks  = 0;
        node->count  = 1 + 1;
        node->state  = FSTATE_NEW;

        m_nodes[node->uid] = node;
        m_catalog[cid] = ppath;
        m_opened[cid]  = ppath;
        *fn = ppath;
    }
    else {
        if (i == m_catalog.end())
            return ERROR_FILE_NOT_EXISTS;

        node = i->second->node;

        assert(i->second->cid == cid);
        // -
        node->state = FSTATE_OPENED;
        node->count++;
        // -
        m_opened[cid] = i->second;
        *fn = i->second;
    }
    return 0;
}

//-----------------------------------------------------------------------------
bool file_database_t::check_existing(const char* path, size_t len) const {
    return m_catalog.find(file_database_t::path_hash(path, len)) != m_catalog.end();
}
//-----------------------------------------------------------------------------
file_path_t* file_database_t::file_by_cid(ui64 cid) const {
    file_catalog_t::const_iterator i = m_catalog.find(cid);
    // -
    if (i != m_catalog.end())
        return i->second;
    return NULL;
}
//-----------------------------------------------------------------------------
file_node_t* file_database_t::file_by_id(const fileid_t uid) const {
    file_map_t::const_iterator i = m_nodes.find(uid);
    // -
    if (i != m_nodes.end())
        return i->second;
    return NULL;
}
//-----------------------------------------------------------------------------
int file_database_t::file_unlink(const char* path, size_t len) {
    const fileid_t uid = file_database_t::path_hash(path, len);
    file_catalog_t::iterator i = m_catalog.find(uid);
    // -
    if (i != m_catalog.end()) {
        file_node_t* node = i->second->node;
        // 1. Удалить запись из каталога (сам файл физически продолжает существовать)
        m_catalog.erase(uid);
        // -
        this->release_node(node); // DELETED || UNREFERENCED
        return 0;
    }
    return -1;
}
/*
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
*/
