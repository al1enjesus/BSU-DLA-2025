#include <stdio.h>
#include <stdlib.h> // <-- Обязательно для realpath, хотя тут не используется
#include <string.h>
#include <time.h>   // <-- НЕОБХОДИМО ДЛЯ struct timespec
#include <unistd.h>

#include "operations.h"

// Глобальная переменная для базовой директории
char *source_dir = NULL;

/**
 * @brief Преобразует относительный FUSE путь в абсолютный путь реального файла.
 * * @param fullpath Буфер для записи абсолютного пути.
 * @param path Относительный FUSE путь (начинается с '/').
 */
void get_full_path(char *fullpath, const char *path) {
    // Длина базовой директории + длина относительного пути + завершающий ноль
    size_t len = strlen(source_dir);
    
    // Если path - это просто "/", то fullpath = source_dir
    if (strcmp(path, "/") == 0) {
        strcpy(fullpath, source_dir);
        return;
    }

    // Если source_dir заканчивается на '/', удаляем его.
    if (source_dir[len - 1] == '/') {
        len--;
    }
    
    // Копируем базовую директорию
    strncpy(fullpath, source_dir, len);
    fullpath[len] = '\0';

    // Добавляем относительный путь. 
    // Заметьте, что path уже начинается с '/'
    strcat(fullpath, path);
}

/**
 * @brief Логирует операцию в stderr.
 * * @param op_name Имя операции (например, "READ", "CREATE").
 * @param path Относительный путь FUSE.
 * @param result Код возврата операции (0 для успеха, -errno при ошибке).
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