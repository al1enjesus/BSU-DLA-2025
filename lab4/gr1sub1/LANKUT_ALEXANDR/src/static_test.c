#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

int main() {
    if (mkdir("logs", 0755) == -1 && errno != EEXIST) {
        perror("Failed to create logs directory");
        return 1;
    }

    printf("Opening test file...\n");
    int fd = open("logs/testfile.txt", O_RDONLY);
    printf("open() returned: %d\n", fd);
    if (fd >= 0) {
        close(fd);
    }
    return 0;
}