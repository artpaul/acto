
#if !defined __libsocket_h__
#define __libsocket_h__

#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//
// Constatnts definition
//

/// Command timeout
#define SOEVENT_TIMEOUT  0x01
/// Accept new income connection
#define SOEVENT_ACCEPTED 0x02
/// Socket ready for reading
#define SOEVENT_READ     0x03
/// Socket ready for writing
#define SOEVENT_WRITE    0x04
/// Client close connection
#define SOEVENT_CLOSED   0x05
///
#define SOEVENT_KEEP


#define SOINFINITY      0

//
// Type definitions
//

typedef struct _stlisten {
    int                 client; // Socket handler
    struct sockaddr_in  src;    //
} SORESULT_LISTEN;

typedef struct _sttimeout {
    void*       dummy;
} SORESULT_TIMEOUT;


typedef struct _soevent {
    const int     type;
    const void*   data;
    void*         param;
} SOEVENT;

// -
typedef void (*socallback)(int s, SOEVENT* const ev);
///
typedef void (*soloopcallback)(void* param);


#ifdef __cplusplus
extern "C" {
#endif

// -
int    so_bind(int s, unsigned long addr, unsigned short port);

// -
int    so_broadcast(int s, unsigned short port, const char* buf, int len);

// -
int    so_close(int s);

/// Connect to remote socket
int    so_connect(int s, unsigned long addr, unsigned short port);

/// Initialize library
int    so_init();

// -
void   so_listen(int s, unsigned long addr, unsigned short port, int backlog, socallback cb, void* param, int color);

/// \brief Цикл ожидание событий сетевого уровня
///        Можно использовать, если приложение не организует свой собственный цикл
void   so_loop(int timeout, soloopcallback cb, void* param);

// -
void   so_pending(int s, int event, int timeout, socallback cb, void* param, int color);

/// Read data synchronously
/// \return Number of bytes readed or -1 in case of error
int    so_readsync(int s, void* buf, size_t size, int timeout);

int    so_sendsync(int s, const void* buf, size_t size);

// Register handler for specific events
// void so_register(int s, int event, socallback cb, void* param, int color);
/// Create new socket
int    so_socket(int type);

/// Terminate library
void   so_terminate();

/// Set user timer
/// \return timer handle
uint64_t so_timer(int timeout, socallback cb, void* param, int color);

#ifdef __cplusplus
}; // extern "C"
#endif

#endif // __libsocket_h__
