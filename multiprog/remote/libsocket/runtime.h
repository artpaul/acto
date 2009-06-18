
#if !defined __libsocket__runtime_h__
#define __libsocket__runtime_h__

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef void (*rtcallback)(int s, SOEVENT* const ev);


// Command's flags

#define SOFLAG_READ     0x0001
#define SOFLAG_WRITE    0x0002


// Command's codes

#define SOCODE_ACCEPT   0x0100
#define SOCODE_PENDING  0x0200
#define SOCODE_TIMER    0x0300


typedef struct _socommand {
    void*               param;       // User param
    struct _socommand*  next;        // Intrusive link
    struct _socommand*  prev;
    rtcallback          callback;    //
    int                 s;           // Socket
    int                 sec_timeout; //
    unsigned short      code;        //
} SOCOMMAND;


// Allocate new command
SOCOMMAND* so_alloc();
// -
int  so_enqueue(SOCOMMAND* cmd);
///
void so_rtloop(int timeout, soloopcallback cb, void* param);

#endif // __libsocket__runtime_h__
