
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

typedef acto::intrusive_queue_t<SOCOMMAND>    Queue;

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


/** Описание группы событий, обслуживаемых в одном потоке */
typedef struct _cluster {
    Queue                   gate;       // Mutex protected queue
    std::set<SOCOMMAND*>    reads;
    Queue                   writes;
    struct timeitem*        timeouts;
    FileDescriptors         fds;        //

    pthread_t               thread;
    pthread_mutex_t         mutex;
    volatile long           active;

    _cluster() {
        timeouts  = 0;
        thread    = 0;
        active    = 1;
        fds.fds   = 0;
        fds.count = 0;
    }
} Cluster;


/**
 * Ядро библиотеки сокетов
 */
class so_runtime_t {
    typedef acto::core::thread_t    ThreadType;

    volatile long               m_active;
    Queue                       m_queue;
    acto::core::mutex_t         m_mutex;
    std::auto_ptr<ThreadType>   m_thread;

private:
    //-------------------------------------------------------------------------
    void clearCluster(Cluster* const ctx) {
        if (ctx->fds.fds != 0)
            free(ctx->fds.fds);

        ctx->fds.fds   = 0;
        ctx->fds.count = 0;
    }
    //-------------------------------------------------------------------------
    void checkTimeQueue(Cluster* cluster) {
        timeitem* const tq = cluster->timeouts;

        if (tq != 0) {
            timeitem* prev = tq;
            timeitem* tl   = tq->next;
            // -
            do {
                if (tl->deadline < time(0)) {
                    timeitem* tmp = tl;
                    SOEVENT   ev  = {SOEVENT_TIMEOUT, 0, tl->cmd->param};
                    // -
                    tl->cmd->callback(tl->cmd->s, &ev);
                    // Remove item
                    if (tl->next == tl) {
                        tl                = 0;
                        cluster->timeouts = 0;
                    }
                    else {
                        prev->next = tl->next;
                        tl         = tl->next;
                        // -
                        if (tmp == tq)
                            cluster->timeouts = tl;
                    }
                    // - заменить на map - так можно быстро удалять
                    //   элемент и получать общий размер элементов
                    //!!! cluster->reads.remove(tmp->cmd);
                    free(tmp->cmd);
                    free(tmp);
                }
                else {
                    prev = tl;
                    tl   = tl->next;
                }
            } while (tl != cluster->timeouts);
        }
    }
    //-------------------------------------------------------------------------
    struct pollfd* relocateDescriptors(Cluster* ctx, size_t count) {
        if (ctx->fds.fds == 0 or ctx->fds.count < count or (ctx->fds.count > count << 1)) {
            if (ctx->fds.fds != 0)
                free(ctx->fds.fds);

            ctx->fds.fds   = (struct pollfd*)malloc(sizeof(struct pollfd) * count);
            ctx->fds.count = count;
        }
        return ctx->fds.fds;
    }
    //-------------------------------------------------------------------------
    SOCOMMAND* processRead(Cluster* cluster, SOCOMMAND* const cmd) {
        SOCOMMAND* result = cmd;

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
                char      buf[16];
                const int rval = recv(cmd->s, buf, sizeof(buf), MSG_PEEK);
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
                timeQueueRemove(cluster, cmd);
                /// !!! result = cluster->reads.remove(cmd);
                free(cmd);
            }
            break;
        }
        return result;
    }
    //-------------------------------------------------------------------------
    void timeQueueRemove(Cluster* const ctx, SOCOMMAND* cmd) {
        if (ctx->timeouts == 0 or ctx->timeouts->next == 0)
            return;
        // -
        struct timeitem* prev = ctx->timeouts;
        struct timeitem* tl   = ctx->timeouts->next;
        // -
        do {
            if (tl->cmd == cmd) {
                struct timeitem* tmp = tl;
                // Remove item
                if (tmp->next == tmp) {
                    tl            = 0;
                    ctx->timeouts = 0;
                }
                else {
                    prev->next = tl->next;
                    tl         = tl->next;
                }
                free(tmp);
                break;
            }
            else {
                prev = tl;
                tl   = tl->next;
            }
        } while (tl != ctx->timeouts);
    }
    //-------------------------------------------------------------------------
    void execute(void* param) {
        Cluster     cluster;
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
                    if (cluster.timeouts) {
                        item->next = cluster.timeouts->next;
                        cluster.timeouts->next = item;
                    }
                    else {
                        item->next       = item;
                        cluster.timeouts = item;
                    }
                }

                if (cmd->code & SOFLAG_READ)
                    cluster.reads.insert(cmd);
                    //cluster.reads.push(cmd);
                else
                    if (cmd->code & SOFLAG_WRITE)
                        cluster.writes.push(cmd);
                cmd = next;
            }

            //
            // Reading
            //
            if (cluster.reads.front())  {
                struct pollfd* const fds = relocateDescriptors(&cluster, cluster.reads.size());
                int        rval;
                int        i = 0;
                SOCOMMAND* p = cluster.reads.front();


                while (p != 0) {
                    fds[i].fd     = p->s;
                    fds[i].events = POLLIN;
                    p = p->next;
                    i++;
                }

                rval = poll(fds, cluster.reads.size(), 300);
                if (rval == -1) {
                    printf("error poll()\n");
                }
                else if (rval > 0) {
                    p = cluster.reads.front();
                    i = 0;
                    while (p != 0) {
                        if (fds[i].revents & POLLIN)
                            p = processRead(&cluster, p);
                        if (p != 0)
                            p = p->next;
                        i++;
                    }
                }
            }

            //
            // Writing
            //
            if (cluster.writes.front())  {
                struct pollfd* const fds = relocateDescriptors(&cluster, cluster.writes.size());
                int         rval;
                int         i = 0;
                SOCOMMAND*  p = cluster.writes.front();

                while (p != 0) {
                    fds[i].fd     = p->s;
                    fds[i].events = POLLOUT;
                    p = p->next;
                    i++;
                }

                rval = poll(fds, cluster.writes.size(), 300);
                if (rval == -1) {
                    printf("error poll()\n");
                }
                else if (rval > 0) {
                    p = cluster.writes.front();
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
            checkTimeQueue(&cluster);
        }
        clearCluster(&cluster);
    }

public:
    so_runtime_t()
        : m_active(0)
    {
        m_thread.reset(new ThreadType(fastdelegate::MakeDelegate(this, &so_runtime_t::execute), NULL));
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
        m_mutex.acquire();
        m_queue.push(cmd);
        m_mutex.release();

        return 0;
    }

    void loop(int timeout, soloopcallback cb, void* param) {
        while (m_active) {
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
