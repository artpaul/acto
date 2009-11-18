
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <client/zfslib.h>

#define MASTER_CLIENTPORT   21581
#define MASTER_IP           "127.0.0.1"

const char* text = "if (zflock(\"test\", ZFF_LOCKSNAPSHOT, 0) != 0)\n"
                   "if (zflock(\"test\", ZFF_LOCKSNAPSHOT, 0) != 0)\n"
                   "if (zflock(\"test\", ZFF_LOCKSNAPSHOT, 0) != 0)\n"
                   "if (zflock(\"test\", ZFF_LOCKSNAPSHOT, 0) != 0)\n";

int main() {
    using namespace zfs;

    zeusfs_t        fs;
    zfs_handle_t*   fd = 0;

    // Initialize library
    if (fs.connect(MASTER_IP, MASTER_CLIENTPORT) != 0) {
        fprintf(stderr, "Cannot connect to file server: %d.\n", fs.error());
        return 1;
    }

    // Создать файл и записать данные
    if ((fd = fs.open("/tmp/text.ascii", ZFS_CREATE | ZFS_EXCLUSIVE | ZFS_APPEND))) {
        for (int i = 0; i < 50; ++i)
            fs.append(fd, text, strlen(text));
        //exit(EXIT_SUCCESS);
        fprintf(stderr, "close\n");
        // -
        fs.close(fd);
    }
    else {
        fprintf(stderr, "open error\n");
        // определить тип ошибки
        //exit(EXIT_SUCCESS);
    }

    // Открыть только-что созданный файл и прочитать данные
    if ((fd = fs.open("/tmp/text.ascii", ZFS_READ | ZFS_SHARED))) {
        assert(fd);
        char    buf[5 * DEFAULT_FILE_BLOCK];
        // -
        while (true) {
            memset(buf, 0, sizeof(buf));
            int rval = fs.read(fd, buf, sizeof(buf)-1);
            if (rval <= 0)
                break;
            printf("%s", buf);
        }
        printf("\n");
        // -
        fs.close(fd);
    }
    return 0;
}
