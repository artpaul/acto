
#ifndef __master_filetree_h__
#define __master_filetree_h__

typedef std::list<cl::string> PathParts;

struct FileInfo;


enum NodeType {
    ntDirectory,
    ntFile
};

enum LockType {
    LOCK_NONE,
    LOCK_READ,
    LOCK_WRITE
};

struct TFileNode {
    FileInfo*               map;       //
    cl::string              name;      //
    std::list<TFileNode*>   children;  //
    NodeType                type;
};

/**
 * Дерево файловой системы
 */
class TFileTree {
private:
    typedef void (*AddNodeHandler)(cl::const_char_iterator, TFileNode*, void*);

    TFileNode       root;
    AddNodeHandler  m_onnewnode;

    bool parsePath(cl::const_char_iterator path, PathParts& parts) const {
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

public:
    TFileTree()
        : m_onnewnode(0)
    {
    }

public:
    /// Добавить новый узел к дереву
    TFileNode* AddPath(cl::const_char_iterator path, LockType lock, NodeType nt, void* param = 0) {
        PathParts parts;
        // -
        if (!parsePath(path, parts))
            return 0;
        TFileNode* node = &root;
        PathParts::const_iterator j = parts.begin();
        // -
        while (j != parts.end()) {
            bool hasPart = false;
            for (std::list<TFileNode*>::iterator i = node->children.begin(); i != node->children.end(); ++i) {
                if ((*i)->name == *j) {
                    j++;
                    node = (*i);
                    hasPart = true;
                    break;
                }
            }
            if (!hasPart) {
                TFileNode* const nn = new TFileNode();

                nn->name = *j;
                nn->map  = 0;
                nn->type = ntDirectory;

                if (m_onnewnode)
                    m_onnewnode(path, nn, param);

                node->children.push_back(nn);
                node = nn;
                j++;
            }
            if (j != parts.end() && node->type == ntFile)
                // Текущий компонент пути файл, а значит он не может быть не
                // конечным элементом пути
                return 0;
        }
        if (node != &root)
            node->type = nt;
        return node;
    }

    bool checkExisting(cl::const_char_iterator path) const {
        return this->findPath(path) != 0;
    }

    TFileNode* findPath(cl::const_char_iterator path) const {
        PathParts parts;
        // -
        if (!parsePath(path, parts))
            return 0;
        // -
        const TFileNode* node = &root;
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
                return 0;
            ++j;
        }
        return node != 0 && j == parts.end() ? const_cast<TFileNode*>(node) : 0;
    }

    void onNewNode(const AddNodeHandler handler) {
        m_onnewnode = handler;
    }
};

#endif // __master_filetree_h__
