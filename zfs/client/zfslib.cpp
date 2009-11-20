
#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <assert.h>

#include <remote/libsocket/libsocket.h>
#include <port/strings.h>

#include <rpc/rpc.h>

#include "zfslib.h"


#define CHUNK_CLIENTPORT    32543
#define MASTER_CLIENTPORT   21581

#define MASTER_IP           "127.0.0.1"
#define CHUNK_IP            "127.0.0.1"

// establish server connection
// send commands
// recieve answers

// establish connection to nodes
// recieve data

namespace zfs {

/// -
struct zfs_handle_t {
    /// !!! Должен быть список узлов, на котором расположены данные
    ui64        cid;        // Каталожный идентификатор
    ui64        uid;        // Файловый идентификатор
    mode_t      mode;
    uint64_t    offset;     //
    int         s;
    // data // блок данных, которые были прочитаны с сервера, но не востребованы клиентом
};


// INTERFACE

////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
zeusfs_t::zeusfs_t() :
    connected(0),
    m_error(0),
    fdmaster(0),
    m_sid(0)
{
    so_init();
}
//------------------------------------------------------------------------------
zeusfs_t::~zeusfs_t() {
    this->disconnect();
    so_terminate();
}
//------------------------------------------------------------------------------
bool zeusfs_t::send_node_open(sockaddr_in nodeip, int port, fileid_t stream, mode_t mode, zfs_handle_t** nc) {
    assert(stream != 0);

    int s = so_socket(SOCK_STREAM, 0);
    fprintf(stderr, "addr: %s\tport: %i\n", inet_ntoa(nodeip.sin_addr), port);
    if (so_connect(s, nodeip.sin_addr.s_addr, port) == 0) {
        TOpenChunkRequest    req;
        req.code   = RPC_FILE_OPEN;
        req.size   = sizeof(req);
        req.client = m_sid;
        req.stream = stream;
        req.mode   = mode;
        // -
        so_sendsync(s, &req, sizeof(req));
        {
            TOpenChunkResponse   rsp;
            so_readsync(s, &rsp, sizeof(rsp), 5);
            if (rsp.file == stream) {
                zfs_handle_t* const conn = new zfs_handle_t();
                conn->uid    = rsp.file;
                conn->mode   = mode;
                conn->offset = 0;
                conn->s      = s;

                *nc = conn;
                return true;
            }
        }
        so_close(s);
    }
    else
        fprintf(stderr, "connection error: %d (%s)\n", errno, strerror(errno));
    *nc = 0;
    return false;
}

//------------------------------------------------------------------------------
int zeusfs_t::append(zfs_handle_t* fd, const char* buf, size_t size) {
    assert(fd && fd->uid);

    TAppendRequest req;
    // -
    req.code   = RPC_FILE_APPEND;
    req.size   = sizeof(TAppendRequest) + size;
    req.stream = fd->uid;
    req.crc    = 0;
    req.bytes  = size;
    req.futher = false;
    // -

    send(fd->s, &req, sizeof(TAppendRequest), 0);
    send(fd->s, buf,  size, 0);
    // -
    {
        TMessage resp;
        // -
        so_readsync(fd->s, &resp, sizeof(resp), 5);
    }
}
//------------------------------------------------------------------------------
int zeusfs_t::close(zfs_handle_t* fd) {
    if (!fd || !fd->uid)
        return -1;
    // -
    rpc_close_file_t req;
    // -
    req.code   = RPC_FILE_CLOSE;
    req.size   = sizeof(req);
    req.client = m_sid;
    req.cid    = fd->cid;
    req.uid    = fd->uid;
    // -
    so_sendsync(fdmaster, &req, sizeof(req));
    // -
    {
        rpc_message_t rsp;
        // -
        so_readsync(fdmaster, &rsp, sizeof(rsp), 5);
    }
    // -
    file_map_t::iterator i = m_files.find(fd->cid);
    if (i != m_files.end()) {
        // SEND CLOSE TO CHUNK;
        so_close(fd->s);
        delete i->second;
        m_files.erase(i);
    }

    return 0;
}
//------------------------------------------------------------------------------
int zeusfs_t::connect(const char* ip, unsigned short port) {
    if (!fdmaster) {
        fdmaster = so_socket(SOCK_STREAM);
        // -
        if (so_connect(fdmaster, inet_addr(ip), port) != 0)
            goto lberror;
        else {
            TMasterSession   req;
            // -
            req.size  = sizeof(req);
            req.code  = RPC_CLIENT_CONNECT;
            req.error = 0;
            req.sid   = 0;
            so_sendsync(fdmaster, &req, sizeof(req));
            // -
            so_readsync(fdmaster, &req, sizeof(req), 5);
            if (req.sid != 0) {
                m_sid = req.sid;
                connected = 1;
                return 0;
            }
        }
lberror:
        m_error = EZFS_CONNECTION;
        if (fdmaster) {
            so_close(fdmaster);
            fdmaster = 0;
        }
    }
    return 1;
}
//------------------------------------------------------------------------------
void zeusfs_t::disconnect() {
    if (fdmaster != 0) {
        ClientCloseSession  req;

        req.code   = RPC_CLOSESESSION;
        req.size   = sizeof(req);
        req.error  = 0;
        req.client = m_sid;
        send(fdmaster, &req, sizeof(req), 0);

        so_close(fdmaster), fdmaster = 0;
    }
}
//------------------------------------------------------------------------------
int zeusfs_t::lock(const char* name, mode_t mode, int wait) {
    return -1;
}
//------------------------------------------------------------------------------
zfs_handle_t* zeusfs_t::open(const char* name, mode_t mode) {
    if (!connected)
        return 0;
    //
    rpc_open_request_t  req;

    req.code   = RPC_FILE_OPEN;
    req.size   = sizeof(req);
    req.client = m_sid;
    req.mode   = mode;
    req.length = strlen(name);
    strcpy(req.path, name);

    so_sendsync(fdmaster, &req, sizeof(req));

    {
        rpc_open_response_t rsp;
        int rval = so_readsync(fdmaster, &rsp, sizeof(rsp), 5);
        // -
        if (rval > 0) {
            if (rsp.error != 0)
                fprintf(stderr, "%s\n", rpc_error_string(rsp.error));
            else {
                zfs_handle_t*  nc = 0;
                // установить соединение с node для получения данных
                if (send_node_open(rsp.nodeip, rsp.nodeport, rsp.uid, mode, &nc)) {
                    nc->cid = rsp.cid;
                    m_files[rsp.cid] = nc;
                    return nc;
                }
            }
        }
    }
    return 0;
}
//-----------------------------------------------------------------------------
int zeusfs_t::read(zfs_handle_t* fd, void* buf, size_t size) {
    assert(fd && fd->uid);

    if (!buf || !size)
        return 0;

    TReadReqest     req;
    // -
    req.code   = RPC_FILE_READ;
    req.size   = sizeof(TReadReqest);
    req.stream = fd->uid;
    req.offset = fd->offset;
    req.bytes  = size;
    // -
    so_sendsync(fd->s, &req, sizeof(TReadReqest)); // POST REQUEST

    TReadResponse       rsp;

    //do {
        int rval = so_readsync(fd->s, &rsp, sizeof(TReadResponse), 5);

        if (rval <= 0)
            return -1;
        else {
            if (!rsp.bytes) {
                assert(rsp.size == sizeof(rsp) && !rsp.futher);
                return 0;
            }
            else {
                if (size >= rsp.bytes) {
                    rval = so_readsync(fd->s, buf, rsp.bytes, 5);
                    if (rval >= 0) {
                        fd->offset += rsp.bytes;
                        return rval;
                    }
                }
                else {
                    cl::array_ptr<char> replay(new char[rsp.bytes]);

                    rval = so_readsync(fd->s, replay.get(), rsp.bytes, 5);
                    if (rval >= 0) {
                        memcpy(buf, replay.get(), size < rsp.bytes ? size : rsp.bytes);
                        fd->offset += size;//rsp.bytes;
                        return rval;
                    }
                }
            }
        }
    //} while (0/*rr.futher*/);

    return -1;
}

int zeusfs_t::remove(const char* path) {
    if (connected) {
        rpc_file_unlink_t   req;
        rpc_message_t       rsp;
        int                 rval;

        req.code   = RPC_FILE_UNLINK;
        req.size   = sizeof(req);
        req.client = m_sid;
        req.length = strlen(path);
        strcpy(req.path, path);

        so_sendsync(fdmaster, &req, sizeof(req));

        // Ответ
        rval = so_readsync(fdmaster, &rsp, sizeof(rsp), 5);

        if (rval == sizeof(rsp) && !rsp.error)
            return 0;
    }
    return -1;
}

}; // namespace zfs
