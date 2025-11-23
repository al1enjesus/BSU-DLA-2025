#include "operations.h"

// Глобальная переменная для базовой директории
char *base_path = NULL;

// Функция логирования операций
void log_operation(const char *op, const char *path, int result) {
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    fprintf(stderr, "[%s] %s: %s (result: %d)\n", timestamp, op, path, result);
}

// Построить полный путь: base_path + relative_path
void get_full_path(char *fullpath, const char *path) {
    strcpy(fullpath, base_path);
    strcat(fullpath, path);
}

// ROT13 преобразование для задания B
void rot13_transform(char *buf, size_t size) {
    for (size_t i = 0; i < size; i++) {
        char c = buf[i];
        if (c >= 'A' && c <= 'Z') {
            buf[i] = ((c - 'A' + 13) % 26) + 'A';
        } else if (c >= 'a' && c <= 'z') {
            buf[i] = ((c - 'a' + 13) % 26) + 'a';
        }
        // Остальные символы остаются без изменений
    }
}

// Uppercase преобразование для задания C
void uppercase_transform(char *buf, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (buf[i] >= 'a' && buf[i] <= 'z') {
            buf[i] = buf[i] - 'a' + 'A';
        }
    }
}