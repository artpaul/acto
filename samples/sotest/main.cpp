
#include <memory.h>
#include <stdio.h>
#include <remote/libsocket/libsocket.h>

#define CLIENTPORT    25121

static void read_data(int s, SOEVENT* const ev) {
    switch (ev->type) {
    case SOEVENT_CLOSED:
        printf("closed\n");
        return;
    case SOEVENT_TIMEOUT:
        printf("time out\n");
        break;
    case SOEVENT_READ:
        {
            char data[512];
            memset(data, 0, sizeof(data));
            int bytes = so_readsync(s, data, sizeof(data), 5);
            if (bytes > 0)
                printf("%s\n", data);
            //so_closesocket(s);
            // return;
        }
        break;
    }
    so_pending(s, SOEVENT_READ, 10, &read_data, ev->param, 0);
}

static void doConnected(int s, SOEVENT* const ev) {
    switch (ev->type) {
    case SOEVENT_ACCEPTED:
        so_pending(((SORESULT_LISTEN*)ev->data)->client, SOEVENT_READ, 10, &read_data, ev->param, 0);
        break;
    case SOEVENT_TIMEOUT:
        printf("accept timeout\n");
        break;
    }
}

int main(int argc, char* argv[]) {
    so_init();

    int fd = so_socket(SOCK_STREAM);

    if (fd != -1) {
        so_listen(fd, inet_addr(argv[1]), CLIENTPORT,  5, &doConnected, 0, 0);

        so_loop(-1, 0, 0);
    }
    else
        printf("create error\n");

    so_terminate();
    return 0;
}
