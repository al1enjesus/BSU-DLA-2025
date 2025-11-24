#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(void) {
    printf("Hello world (stdout)\\n");
    int fd = open("/etc/hosts", O_RDONLY);
    if (fd >= 0) {
        char buf[16];
        read(fd, buf, sizeof(buf));
        close(fd);
    }
    return 0;
}
