
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
    fileid_t    id;
    mode_t      mode;
    uint64_t    offset;    //
    int         s;
};


// INTERFACE

////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------------------------
TZeusFS::TZeusFS() :
    connected(0),
    m_error(0),
    fdmaster(0),
    m_sid(0)
{
    so_init();
}
//------------------------------------------------------------------------------
TZeusFS::~TZeusFS() {
    this->disconnect();
    so_terminate();
}
//------------------------------------------------------------------------------
bool TZeusFS::SendOpenToNode(sockaddr_in nodeip, fileid_t stream, mode_t mode, zfs_handle_t** nc) {
    assert(stream != 0);

    int s = so_socket(SOCK_STREAM);
    printf("addr: %s\n", inet_ntoa(nodeip.sin_addr));
    if (so_connect(s, /*inet_addr(CHUNK_IP)*/nodeip.sin_addr.s_addr, CHUNK_CLIENTPORT) == 0) {
        OpenChunkRequest    req;
        req.code   = RPC_OPENFILE;
        req.size   = sizeof(req);
        req.client = m_sid;
        req.stream = stream;
        req.mode   = mode;
        // -
        so_sendsync(s, &req, sizeof(req));
        {
            OpenChunkResponse   rsp;
            so_readsync(s, &rsp, sizeof(rsp), 5);
            if (rsp.file == stream) {
                zfs_handle_t* const conn = new zfs_handle_t();
                conn->id     = rsp.file;
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
        printf("connection error: %d\n", errno);
    *nc = 0;
    return false;
}

//------------------------------------------------------------------------------
int TZeusFS::Append(zfs_handle_t* fd, const char* buf, size_t size) {
    assert(fd && fd->id);

    AppendRequest req;
    // -
    req.code   = RPC_APPEND;
    req.size   = sizeof(AppendRequest) + size;
    req.stream = fd->id;
    req.crc    = 0;
    req.bytes  = size;
    req.futher = false;
    // -

    send(fd->s, &req, sizeof(AppendRequest), 0);
    send(fd->s, buf,  size, 0);
    // -
    {
        TCommonResponse resp;
        // -
        so_readsync(fd->s, &resp, sizeof(resp), 5);
    }
}
//------------------------------------------------------------------------------
int TZeusFS::Close(zfs_handle_t* fd) {
    if (!fd || !fd->id)
        return -1;
    // -
    CloseRequest   req;
    // -
    req.code   = RPC_CLOSE;
    req.size   = sizeof(CloseRequest);
    req.stream = fd->id;
    // -
    so_sendsync(fdmaster, &req, sizeof(CloseRequest));
    // -
    {
        TCommonResponse resp;
        // -
        so_readsync(fdmaster, &resp, sizeof(resp), 5);
    }
    // -
    TStreamMap::iterator i = m_streams.find(fd->id);
    if (i != m_streams.end()) {
        // SEND CLOSE TO CHUNK;
        so_close(fd->s);
        delete i->second;
        m_streams.erase(i);
    }

    return 0;
}
//------------------------------------------------------------------------------
int TZeusFS::connect(const char* ip, unsigned short port) {
    if (!fdmaster) {
        fdmaster = so_socket(SOCK_STREAM);
        // -
        if (so_connect(fdmaster, inet_addr(ip), port) != 0)
            goto lberror;
        else {
            MasterSession   req;
            // -
            req.sid = 0;
            send(fdmaster, &req, sizeof(req), 0);
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
void TZeusFS::disconnect() {
    if (fdmaster != 0) {
        ClientCloseSession  req;

        req.code   = RPC_CLOSESESSION;
        req.size   = sizeof(req);
        req.client = m_sid;
        send(fdmaster, &req, sizeof(req), 0);

        so_close(fdmaster), fdmaster = 0;
    }
}
//------------------------------------------------------------------------------
int TZeusFS::lock(const char* name, mode_t mode, int wait) {
    return 0;
}
//------------------------------------------------------------------------------
zfs_handle_t* TZeusFS::Open(const char* name, mode_t mode) {
    if (not connected)
        return 0;
    //
    const size_t len = strlen(name);
    OpenRequest  req;

    req.code   = RPC_OPENFILE;
    req.size   = sizeof(OpenRequest) + len;
    req.client = m_sid;
    req.mode   = mode;
    req.length = len;

    so_sendsync(fdmaster, &req, sizeof(OpenRequest));
    so_sendsync(fdmaster, name, len);

    {
        OpenResponse  rsp;
        int           rval = so_readsync(fdmaster, &rsp, sizeof(OpenResponse), 5);
        // -
        if (rval > 0) {
            if (rsp.err != 0 || rsp.stream == 0)
                printf("%s\n", rpcErrorString(rsp.err));
            else {
                zfs_handle_t*  nc = 0;
                // установить соединение с node для получения данных
                if (SendOpenToNode(rsp.nodeip, rsp.stream, mode, &nc)) {
                    m_streams[rsp.stream] = nc;
                    return nc;
                }
            }
        }
    }
    return 0;
}
//------------------------------------------------------------------------------
int TZeusFS::Read(zfs_handle_t* fd, void* buf, size_t size) {
    assert(fd && fd->id);

    TReadReqest     req;
    // -
    req.code   = RPC_READ;
    req.size   = sizeof(TReadReqest);
    req.stream = fd->id;
    req.offset = fd->offset;
    req.bytes  = size;
    // -
    send(fd->s, &req, sizeof(TReadReqest), 0); // POST REQUEST

    {
        ReadResponse    rr;
        int             rval = so_readsync(fd->s, &rr, sizeof(ReadResponse), 5);

        if (rval > 0 && rr.size > 0) {
            cl::array_ptr<char> replay(new char[rr.size]);
            // -
            rval = so_readsync(fd->s, replay.get(), rr.size, 5);
            if (rval >= 0) {
                memcpy(buf, replay.get(), size < rr.size ? size : rr.size);
                fd->offset += rr.size;
                return rval;
            }
        }
    }
    return -1;
}

}; // namespace zfs
