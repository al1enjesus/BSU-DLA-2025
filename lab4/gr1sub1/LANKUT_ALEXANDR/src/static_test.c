#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    printf("Opening test file...\n");
    int fd = open("logs/testfile.txt", O_RDONLY);
    printf("open() returned: %d\n", fd);
    if (fd >= 0) {
        close(fd);
    }
    return 0;
}