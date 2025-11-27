    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <time.h>
    #include <unistd.h>
    #include "operations.h"

    // ... (логика utils.c)
    // Глобальная переменная для базовой директории
    char *source_dir = NULL;

    /**
     * @brief Преобразует относительный FUSE путь в абсолютный путь реального файла.
     */
    void get_full_path(char *fullpath, const char *path) {
        size_t len = strlen(source_dir);
        
        if (strcmp(path, "/") == 0) {
            strcpy(fullpath, source_dir);
            return;
        }

        if (source_dir[len - 1] == '/') {
            len--;
        }
        
        strncpy(fullpath, source_dir, len);
        fullpath[len] = '\0';

        strcat(fullpath, path);
    }

    /**
     * @brief Логирует операцию в stderr.
     */
    void log_operation(const char *op_name, const char *path, int result) {
        time_t timer;
        char buffer[26];
        struct tm* tm_info;

        time(&timer);
        tm_info = localtime(&timer);
        strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);

        // Логирование в требуемом формате
        fprintf(stderr, "[%s] %s: %s (result: %d)\n", buffer, op_name, path, result);
    }