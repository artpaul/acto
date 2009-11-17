
#include <errno.h>
#include <malloc.h>
#include <memory.h>
#include <stdio.h>
#include <time.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <stdint.h>
#include <poll.h>

#include <set>

#include <generic/intrqueue.h>
#include <system/mutex.h>
#include <system/thread.h>

#include "libsocket.h"
#include "runtime.h"

struct timeitem {
    SOCOMMAND*          cmd;
    time_t              deadline;
    struct timeitem*    next;
};


/** */
typedef struct _fdescriptors {
    struct pollfd*  fds;
    size_t          count;
} FileDescriptors;


typedef acto::generics::intrusive_queue_t<SOCOMMAND>  Queue;
typedef acto::generics::intrusive_queue_t<timeitem>   TimeQueue;

/**
 * Описание группы событий, обслуживаемых в одном потоке
 */
class cluster_t {
    std::set<SOCOMMAND*>    m_reads;
    Queue                   writes;
    TimeQueue               timeouts;
    FileDescriptors         fds;        //

public:
    acto::core::mutex_t     m_mutex;
    Queue                   m_queue;    // Mutex protected queue
    volatile long           m_active;

private:
    //-------------------------------------------------------------------------
    void check_timequeue() {
        if (timeouts.empty())
            return;

        timeitem* prev = NULL;
        timeitem* tl   = timeouts.front();
        // -
        do {
            if (tl->deadline < time(0)) {
                timeitem* tmp = tl;
                SOEVENT   ev  = {SOEVENT_TIMEOUT, 0, tl->cmd->param};
                // -
                tl->cmd->callback(tl->cmd->s, &ev);
                // Remove item
                tl = timeouts.remove(tl, prev);
                m_reads.erase(tmp->cmd);
                free(tmp->cmd);
                free(tmp);
            }
            else {
                prev = tl;
                tl   = tl->next;
            }
        } while (tl != NULL);
    }
    //-------------------------------------------------------------------------
    void process_read(SOCOMMAND* const cmd) {
        switch (cmd->code & ~0x000F) {
        default:
            break;
        case SOCODE_ACCEPT:
            {
                struct sockaddr_in  rem;
                socklen_t  rl = sizeof(rem);
                const int  s  = accept(cmd->s, (struct sockaddr*)&rem, &rl);
                // -
                if (s != -1) {
                    SORESULT_LISTEN res;
                    SOEVENT         ev  = {SOEVENT_ACCEPTED, &res, cmd->param};
                    // -
                    res.client = s;
                    res.src    = rem;
                    // -
                    cmd->callback(cmd->s, &ev);
                }
            }
            break;
        case SOCODE_PENDING:
            {
                unsigned char buf[8];
                const int     rval = recv(cmd->s, buf, sizeof(buf), MSG_PEEK);
                // -
                if (rval > 0) {
                    SOEVENT ev = {SOEVENT_READ, 0, cmd->param};
                    cmd->callback(cmd->s, &ev);
                }
                else {
                    SOEVENT ev = {SOEVENT_CLOSED, 0, cmd->param};
                    cmd->callback(cmd->s, &ev);
                    close(cmd->s);
                }
                // Remove command form queue
                this->remove_timequeue(cmd);
                this->m_reads.erase(cmd);
                free(cmd);
            }
            break;
        }
    }
    //-------------------------------------------------------------------------
    struct pollfd* relocate_descriptors(size_t count) {
        if (fds.fds == 0 || fds.count < count || (fds.count > count << 1)) {
            if (fds.fds != 0)
                free(fds.fds);

            fds.fds   = (struct pollfd*)malloc(sizeof(struct pollfd) * count);
            fds.count = count;
        }
        return fds.fds;
    }
    //-------------------------------------------------------------------------
    void remove_timequeue(SOCOMMAND* cmd) {
        if (timeouts.empty())
            return;
        // -
        struct timeitem* prev = NULL;
        struct timeitem* tl   = timeouts.front();
        // -
        do {
            if (tl->cmd == cmd) {
                struct timeitem* tmp = tl;
                // Remove item
                tl = timeouts.remove(tl, prev);
                free(tmp);
                break;
            }
            else {
                prev = tl;
                tl   = tl->next;
            }
        } while (tl != NULL);
    }

public:
    cluster_t()
        : m_active(1)
    {
        //thread    = 0;
        fds.fds   = 0;
        fds.count = 0;
    }

public:
    //-------------------------------------------------------------------------
    void clear() {
        if (fds.fds != 0)
            free(fds.fds);

        fds.fds   = 0;
        fds.count = 0;
    }
    //-------------------------------------------------------------------------
    void execute(void* /*param*/) {
       //int epfd = epoll_create(10);
        // -
        while (m_active) {
            SOCOMMAND* cmd = 0;
            // Get commands
            m_mutex.acquire();
            cmd = m_queue.detatch();
            m_mutex.release();

            while (cmd != 0) {
                // Сохранить указатель на следующую команду заранее, так как
                // при перестановки команды в другую очередь указатели затираются
                SOCOMMAND* const next = cmd->next;

                if (cmd->sec_timeout != 0) {
                    struct timeitem* item = (struct timeitem*)malloc(sizeof(struct timeitem));
                    time_t           t    = time(0) + cmd->sec_timeout;
                    // Add to time queue
                    item->cmd      = cmd;
                    item->deadline = t;
                    item->next     = 0;
                    this->timeouts.push(item);
                }

                if (cmd->code & SOFLAG_READ) {
                    this->m_reads.insert(cmd);
                }
                else if (cmd->code & SOFLAG_WRITE) {
                    this->writes.push(cmd);
                }
                else
                    free(cmd);
                cmd = next;
            }

            //
            // Reading
            //
            if (!this->m_reads.empty())  {
                typedef std::set<SOCOMMAND*>    cmd_set_t;

                struct pollfd* const fds = this->relocate_descriptors(this->m_reads.size());
                int        rval;
                int        j = 0;

                for (cmd_set_t::const_iterator i = this->m_reads.begin(); i != this->m_reads.end(); ++i) {
                    fds[j].fd     = (*i)->s;
                    fds[j].events = POLLIN;
                    ++j;
                }

                rval = poll(fds, this->m_reads.size(), 300);
                if (rval == -1) {
                    fprintf(stderr, "error poll()\n");
                }
                else if (rval > 0) {
                    j = 0;
                    for (cmd_set_t::const_iterator i = this->m_reads.begin(); i != this->m_reads.end(); ++i) {
                        if (fds[j].revents & POLLIN)
                            this->process_read(*i);
                        ++j;
                    }
                }
            }

            //
            // Writing
            //
            if (this->writes.front())  {
                struct pollfd* const fds = this->relocate_descriptors(this->writes.size());
                int         rval;
                int         i = 0;
                SOCOMMAND*  p = this->writes.front();

                while (p != 0) {
                    fds[i].fd     = p->s;
                    fds[i].events = POLLOUT;
                    p = p->next;
                    i++;
                }

                rval = poll(fds, this->writes.size(), 300);
                if (rval == -1) {
                    fprintf(stderr, "error poll()\n");
                }
                else if (rval > 0) {
                    p = this->writes.front();
                    i = 0;
                    while (p != 0) {
                        if (fds[i].revents & POLLOUT) {
                            //p = processRead(&cluster, p); Write
                        }
                        if (p != 0)
                            p = p->next;
                        i++;
                    }
                }
            }

            // -
            this->check_timequeue();
        }
    }
};


/**
 * Ядро библиотеки сокетов
 */
class so_runtime_t {
    typedef acto::core::thread_t    ThreadType;

    std::auto_ptr<ThreadType>   m_thread;
    cluster_t                   m_cluster;

private:
    //-------------------------------------------------------------------------
    static void execute(void* param) {
        so_runtime_t* const pthis = static_cast< so_runtime_t* >(param);

        pthis->m_cluster.execute(NULL);
        pthis->m_cluster.clear();
    }

public:
    so_runtime_t() {
        m_thread.reset(new ThreadType(&so_runtime_t::execute, this));
    }

    static so_runtime_t* instance() {
        static so_runtime_t    value;

        return &value;
    }

public:
    /// Выделить память под экземпляр команды
    SOCOMMAND* allocate_command() const {
        SOCOMMAND* const rval = (SOCOMMAND*)malloc(sizeof(SOCOMMAND));
        // -
        rval->next        = 0;
        rval->prev        = 0;
        rval->sec_timeout = 0;

        return rval;
    }
    /// Поместить команду в очередь обработки
    int enqueue(SOCOMMAND* const cmd) {
        m_cluster.m_mutex.acquire();
        m_cluster.m_queue.push(cmd);
        m_cluster.m_mutex.release();

        return 0;
    }

    void loop(int timeout, soloopcallback cb, void* param) {
        while (m_cluster.m_active) {
            if (cb != NULL)
                cb(param);
            // -
            usleep(100);
        }
    }
};


//-----------------------------------------------------------------------------
SOCOMMAND* so_alloc() {
    return so_runtime_t::instance()->allocate_command();
}
//-----------------------------------------------------------------------------
int so_enqueue(SOCOMMAND* cmd) {
    return so_runtime_t::instance()->enqueue(cmd);
}
//------------------------------------------------------------------------------
void so_rtloop(int timeout, soloopcallback cb, void* param) {
    so_runtime_t::instance()->loop(timeout, cb, param);
}
