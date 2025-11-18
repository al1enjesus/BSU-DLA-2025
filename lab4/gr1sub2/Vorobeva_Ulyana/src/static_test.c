#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd = open("/etc/hosts", O_RDONLY);
    if (fd >= 0) {
        char buf[128];
        ssize_t n = read(fd, buf, sizeof(buf)-1);
        if (n > 0) {
            buf[n] = 0;
            printf("read %zd bytes from /etc/hosts\n", n);
        }
        close(fd);
    } else {
        perror("open");
    }
    return 0;
}
