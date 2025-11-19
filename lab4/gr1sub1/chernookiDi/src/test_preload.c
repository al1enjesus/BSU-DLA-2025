#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    printf("Testing LD_PRELOAD\n");
    
    // Тест open
    int fd = open("/dev/null", O_RDONLY);
    if (fd >= 0) {
        printf("Opened /dev/null successfully\n");
        close(fd);
    }
    
    // Тест openat  
    fd = openat(AT_FDCWD, "/dev/null", O_RDONLY);
    if (fd >= 0) {
        printf("Opened /dev/null with openat successfully\n");
        close(fd);
    }
    
    return 0;
}