#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main() {
    printf("Starting test program...\n");
    
    // Тестируем open/write/close
    int fd = open("test_output.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd >= 0) {
        write(fd, "Hello from test program!\n", 25);
        close(fd);
    }
    
    // Тестируем open/read/close  
    fd = open("test_output.txt", O_RDONLY);
    if (fd >= 0) {
        char buffer[100];
        ssize_t bytes = read(fd, buffer, sizeof(buffer));
        if (bytes > 0) {
            write(1, buffer, bytes); // stdout
        }
        close(fd);
    }
    
    printf("Test program finished.\n");
    return 0;
}
