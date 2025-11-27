#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "operations.h"

// Глобальная переменная для базовой директории
char *source_dir = NULL;

/**
 * @brief Преобразует относительный FUSE путь в абсолютный путь реального файла.
 * 
 * @param fullpath Буфер для результата (должен быть >= PATH_MAX_LEN)
 * @param path     FUSE путь (например "/file.txt")
 */
void get_full_path(char *fullpath, const char *path) {
    // Обработка корневой директории
    if (strcmp(path, "/") == 0) {
        strcpy(fullpath, source_dir);
        return;
    }

    // Убираем trailing slash из source_dir если есть
    size_t len = strlen(source_dir);
    if (source_dir[len - 1] == '/') {
        len--;
    }

    // Конкатенация: source_dir + path
    strncpy(fullpath, source_dir, len);
    fullpath[len] = '\0';
    strcat(fullpath, path);
}

/**
 * @brief Логирует операцию в stderr в требуемом формате.
 * 
 * Формат: [YYYY-MM-DD HH:MM:SS] OPERATION: path (result: N)
 * 
 * @param op_name  Имя операции (READ, WRITE, CREATE и т.д.)
 * @param path     Путь к файлу/директории
 * @param result   Код возврата (0 = успех, <0 = ошибка)
 */
void log_operation(const char *op_name, const char *path, int result) {
    time_t timer;
    char buffer[26];
    struct tm* tm_info;

    time(&timer);
    tm_info = localtime(&timer);
    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(stderr, "[%s] %s: %s (result: %d)\n", 
            buffer, op_name, path, result);
}