
#ifndef __master_filetree_h__
#define __master_filetree_h__

typedef std::list<cl::string> PathParts;

/**
 * Дерево файловой системы
 */
template <typename FileNode>
class TFileTree {
private:
    typedef void (*AddNodeHandler)(cl::const_char_iterator, FileNode*, void*);

    FileNode        root;
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
    FileNode* addPath(cl::const_char_iterator path, LockType lock, void* param = 0) {
        PathParts parts;
        // -
        if (not parsePath(path, parts))
            return 0;
        FileNode* node = &root;
        PathParts::const_iterator j = parts.begin();
        // -
        while (j != parts.end()) {
            bool hasPart = false;
            for (typename std::list<FileNode*>::iterator i = node->children.begin(); i != node->children.end(); ++i) {
                if ((*i)->name == *j) {
                    j++;
                    node = (*i);
                    hasPart = true;
                    break;
                }
            }
            if (not hasPart) {
                FileNode* const nn = new FileNode();

                nn->name = *j;
                nn->map  = 0;
                nn->type = ntDirectory;

                if (m_onnewnode)
                    m_onnewnode(path, nn, param);

                node->children.push_back(nn);
                node = nn;
                j++;
            }
            if (j != parts.end() and node->type == ntFile)
                // Текущий компонент пути файл, а значит он не может быть не
                // конечным элементом пути
                return 0;
        }
        if (node != &root)
            node->type = ntFile;
        return node;
    }

    bool checkExisting(cl::const_char_iterator path) const {
        return this->findPath(path) != 0;
    }

    FileNode* findPath(cl::const_char_iterator path) const {
        PathParts parts;
        // -
        if (not parsePath(path, parts))
            return 0;
        // -
        const FileNode* node = &root;
        PathParts::const_iterator j = parts.begin();
        // -
        while (node != 0 and j != parts.end()) {
            bool hasPart = false;
            for (typename std::list<FileNode*>::const_iterator i = node->children.begin(); i != node->children.end(); ++i) {
                if ((*i)->name == *j) {
                    node = (*i);
                    hasPart = true;
                    break;
                }
            }
            if (not hasPart)
                return 0;
            ++j;
        }
        return node != 0 and j == parts.end() ? const_cast<FileNode*>(node) : 0;
    }

    void onNewNode(const AddNodeHandler handler) {
        m_onnewnode = handler;
    }
};

#endif // __master_filetree_h__
