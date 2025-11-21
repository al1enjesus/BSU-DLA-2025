#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    printf("Simple program started\n");
    int fd = open("test.txt", O_CREAT | O_WRONLY, 0644);
    if (fd >= 0) {
        write(fd, "Hello", 5);
        close(fd);
    }
    
    printf("Simple program finished\n");
    return 0;
}