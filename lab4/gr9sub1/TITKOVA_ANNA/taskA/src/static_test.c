#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd = open("test_data/test_dir/file0.txt", O_RDONLY);
    if (fd != -1) {
        char buf[1024];
        ssize_t bytes = read(fd, buf, sizeof(buf));
        close(fd);
        printf("Read %zd bytes from file\n", bytes);
    }
    return 0;
}
