
#include <errno.h>
#include <memory.h>
#include <malloc.h>
#include <fcntl.h>
#include <stdio.h>
#include <poll.h>
#include <stdint.h>
#include <time.h>

#include "libsocket.h"
#include "runtime.h"

//-----------------------------------------------------------------------------
int so_bind(int s, unsigned long addr, unsigned short port) {
    struct sockaddr_in  sa;
    // -
    sa.sin_family = AF_INET;
    sa.sin_port   = htons(port);
    sa.sin_addr.s_addr = addr;
    // -
    return bind(s, (const struct sockaddr*)&sa, sizeof(sa));
}
//-----------------------------------------------------------------------------
int so_broadcast(int s, unsigned short port, const char* buf, int len) {
    const int  	opt = 1;
    struct sockaddr_in sa;

    // Enable broadcast
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char*)&opt, sizeof(opt));

    // -
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port   = htons(port);
    sa.sin_addr.s_addr = INADDR_BROADCAST;
    // -
    return sendto(s, buf, len, 0, (struct sockaddr*)&sa, sizeof(sa));
}
//-----------------------------------------------------------------------------
int so_close(int s) {
    return close(s);
}
//-----------------------------------------------------------------------------
int so_connect(int s, unsigned long addr, unsigned short port) {
    struct sockaddr_in  sa;
    int                 rval;

    sa.sin_family      = AF_INET;
    sa.sin_addr.s_addr = addr;
    sa.sin_port        = htons(port);

    rval = connect(s, (const struct sockaddr*)&sa, sizeof(sa));

    if ((rval != 0) && (errno == EINPROGRESS)) {
        pollfd  fd;
        // -
        fd.fd     = s;
        fd.events = POLLOUT;

        while (1) {
            rval = poll(&fd, 1, 100);
            // -
            if (rval > 0) {
                char        opt;
                socklen_t   len = sizeof(opt);

                getsockopt(s, SOL_SOCKET, SO_ERROR, &opt, &len);
                return opt;
            }
            else if (rval == -1 && (errno != EINPROGRESS))
                return -1;
        }
    }
    return rval;
}
//-----------------------------------------------------------------------------
int so_init() {
    return 0;
}
//-----------------------------------------------------------------------------
void so_listen(int s, unsigned long addr, unsigned short port, int backlog, socallback cb, void* param) {
    if (so_bind(s, addr, port) != 0)
        goto error;
    // -
    if (listen(s, backlog) != 0)
        goto error;

    {
        SOCOMMAND* const cmd = so_alloc();
        // -
        if (cmd != 0) {
            cmd->code     = SOCODE_ACCEPT | SOFLAG_READ;
            cmd->callback = cb;
            cmd->param    = param;
            cmd->s        = s;
            // -
            so_enqueue(cmd);
        }
    }
    return;

error:
    printf("so_listen error: %d\n", errno);
}
//------------------------------------------------------------------------------
void so_loop(int timeout, soloopcallback cb, void* param) {
    so_rtloop(timeout, cb, param);
}
//------------------------------------------------------------------------------
void so_pending(int s, int event, int timeout, socallback cb, void* param) {
    SOCOMMAND* const cmd  = so_alloc();
    unsigned short   flag;

    // Map event code to flag code
    if (event == SOEVENT_READ)
        flag = SOFLAG_READ;
    else if (event == SOEVENT_WRITE)
        flag = SOFLAG_WRITE;
    else
        // !!! Incorrect event value
        return;

    // Pending command
    if (cmd != 0) {
        // -
        cmd->code        = SOCODE_PENDING | flag;
        cmd->callback    = cb;
        cmd->param       = param;
        cmd->s           = s;
        cmd->sec_timeout = timeout;
        // -
        so_enqueue(cmd);
    }
}
//------------------------------------------------------------------------------
int so_readsync(int s, void* buf, size_t size, int timeout) {
    pollfd  fd;
    time_t  start  = time(NULL);
    int     readed = 0;
    // -
    fd.fd     = s;
    fd.events = POLLIN;
    // -
    while ((start + timeout) > time(NULL)) {
        const int ret = poll(&fd, 1, timeout * 1000);
        // -
        if (ret > 0) {
            int rval = recv(s, (char*)buf + readed, size, 0);

            if (rval == 0)
                return readed;
            else {
                readed += rval;
                size   -= rval;

                if (size == 0)
                    return readed;
            }
        }
        else if (ret == 0) {
            errno = ETIMEDOUT;
            if (readed == 0)
                return -1;
            else
                return readed;
        }
        else // ret < 0
            return ret;
    }
    return readed;
}
//------------------------------------------------------------------------------
int so_sendsync(int s, const void* buf, size_t size) {
    return send(s, buf, size, 0);
}

//------------------------------------------------------------------------------
int so_socket(int type) {
    int     fd;
    int     protocol;

    // Choose protocol type
    switch (type) {
        default:
            return -1;
        case SOCK_STREAM:
            protocol = IPPROTO_TCP;
            break;
        case SOCK_DGRAM:
            protocol = IPPROTO_UDP;
            break;
    }
    // Create new socket
    fd = socket(AF_INET, type, protocol);
    // Set socket to non-blocking mode
    if (fd != -1) {
        u_long  flags = fcntl(fd, F_GETFL);
        // -
        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
            printf("cannot set nonblocking mode\n");
    }
    // -
    return fd;
}
//-----------------------------------------------------------------------------
void so_terminate() {
    // -
}

/*
 *
 */
uint64_t so_timer(int timeout, socallback cb, void* param) {
    SOCOMMAND* const cmd  = so_alloc();

    // Pending command
    if (cmd != 0) {
        cmd->code        = SOCODE_TIMER;
        cmd->callback    = cb;
        cmd->param       = param;
        cmd->s           = 0;
        cmd->sec_timeout = timeout;
        // -
        so_enqueue(cmd);
    }
    return 0;
}
