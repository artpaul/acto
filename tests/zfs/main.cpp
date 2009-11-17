
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <client/zfslib.h>

#define MASTER_CLIENTPORT   21581
#define MASTER_IP           "127.0.0.1"

const char* text = "if (zflock(\"test\", ZFF_LOCKSNAPSHOT, 0) != 0)\n";

int main() {
    using namespace zfs;

    zeusfs_t        fs;
    zfs_handle_t*   fd = 0;

    // Initialize library
    if (fs.connect(MASTER_IP, MASTER_CLIENTPORT) != 0) {
        printf("Cannot connect to file server: %d.\n", fs.error());
        return 1;
    }

    // Создать файл и записать данные
    if ((fd = fs.open("/tmp/text.ascii", ZFS_CREATE | ZFS_EXCLUSIVE | ZFS_APPEND))) {
        fs.append(fd, text, strlen(text));
        //exit(EXIT_SUCCESS);
        printf("close\n");
        // -
        fs.close(fd);
    }
    else {
        printf("open error\n");
        // определить тип ошибки
        //exit(EXIT_SUCCESS);
    }

    // Открыть только-что созданный файл и прочитать данные
    if ((fd = fs.open("/tmp/text.ascii", ZFS_READ | ZFS_SHARED))) {
        assert(fd);
        char    buf[1024];
        // -
        while (fs.read(fd, buf, sizeof(buf)) != -1) {
            printf("%s\n", buf);
        }
        // -
        fs.close(fd);
    }
    return 0;
}
