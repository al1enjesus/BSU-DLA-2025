#include "operations.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>

/* Глобальные переменные - ИНИЦИАЛИЗИРУЕМ ЗДЕСЬ */
char *base_path = NULL;
volatile sig_atomic_t stop_flag = 0;

/* Обработчик сигнала для graceful shutdown */
void signal_handler(int sig) {  // Убрали static
    (void)sig;
    stop_flag = 1;
}

/* Получить полный путь к файлу */
char* get_full_path(const char *path) {
    if (base_path == NULL) {
        return NULL;
    }
    
    size_t base_len = strlen(base_path);
    size_t path_len = strlen(path);
    size_t full_len = base_len + path_len + 2;
    
    char *fullpath = malloc(full_len);
    if (fullpath == NULL) {
        return NULL;
    }
    
    snprintf(fullpath, full_len, "%s%s", base_path, path);
    return fullpath;
}

/* Логирование операций */
void log_operation(const char *op, const char *path, int result) {
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    fprintf(stderr, "[%s] %s: %s (result: %d)\n", timestamp, op, path, result);
}

/* Логирование операций с байтами */
void log_operation_bytes(const char *op, const char *path, size_t bytes, off_t offset, int result) {
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    fprintf(stderr, "[%s] %s: %s (%zu bytes at offset %ld, result: %d)\n",
            timestamp, op, path, bytes, offset, result);
}

/* Проверка безопасности пути */
int check_path_safety(const char *path) {
    if (path == NULL) return 0;
    
    /* Проверка на path traversal атаки */
    const char *p = path;
    int dot_count = 0;
    
    while (*p) {
        if (*p == '/') {
            dot_count = 0;
        } else if (*p == '.') {
            dot_count++;
            if (dot_count >= 2 && (*(p+1) == '/' || *(p+1) == '\0')) {
                return 0; /* Нашли .. */
            }
        } else {
            dot_count = 0;
        }
        p++;
    }
    
    return 1;
}
