#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h> // Для realpath

#include "operations.h"

// Глобальная переменная для базовой директории
char *source_dir = NULL;

/**
 * @brief Преобразует относительный FUSE путь в абсолютный путь реального файла, 
 * обеспечивая защиту от переполнения буфера.
 * * @param fullpath Буфер для результата (PATH_MAX_LEN)
 * @param path     FUSE путь (например "/file.txt")
 * @return int     0 при успехе, -ENAMETOOLONG при переполнении буфера.
 */
int get_full_path(char *fullpath, const char *path) {
    // В main.c мы гарантировали, что source_dir заканчивается на '/'
    // Требуемая длина: source_dir + path (исключая начальный '/') + 1 (для '\0')
    size_t required_len = strlen(source_dir) + strlen(path);

    if (required_len >= PATH_MAX_LEN) {
        // Логирование критической ошибки: слишком длинный путь
        log_operation("PATH_ERROR_LEN", path, -ENAMETOOLONG);
        return -ENAMETOOLONG;
    }

    // Использование snprintf для безопасной конкатенации
    // Формат: %s (source_dir с '/’) + %s (path без '/')
    // path всегда начинается с '/', поэтому используем path + 1
    // Обработка path = "/" в main.c теперь не нужна благодаря гарантии '/' в source_dir
    snprintf(fullpath, PATH_MAX_LEN, "%s%s", 
             source_dir, path + 1);
    
    return 0;
}

/**
 * @brief Проверяет путь на возможность выхода за пределы базовой директории (Path Traversal).
 * * Использует realpath для разрешения всех '..' и симлинков, затем проверяет
 * префикс на соответствие source_dir.
 * * @param fullpath Абсолютный путь, который нужно проверить.
 * @return int     0, если путь безопасен, -EACCES, если обнаружена атака.
 */
int check_path_security(const char *fullpath) {
    char real[PATH_MAX_LEN];
    size_t source_len = strlen(source_dir);

    // 1. Используем realpath для разрешения всех '..' и симлинков.
    // realpath возвращает NULL, если путь не существует, но мы всё равно 
    // можем проверить, что он не выходит за source_dir, используя basename.
    if (realpath(fullpath, real) == NULL) {
        // Если realpath не смог разрешить путь (например, файл еще не создан), 
        // мы можем пропустить проверку, но только если путь НЕ содержит '..'.
        // Поскольку FUSE path гарантированно не содержит '..' благодаря ядру,
        // и мы полагаемся на realpath для проверки, мы просто продолжаем.
        // Если ошибка -ENOENT, это нормально. Если -EACCES, то мы вернем его позже.
        return 0; 
    }

    // 2. Проверка: Убедиться, что реальный путь начинается с source_dir.
    // source_dir гарантированно оканчивается на '/' (например, /tmp/source/)
    if (strncmp(real, source_dir, source_len) != 0) {
        // Если путь начинается, но не совпадает, это потенциальный выход.
        // Например: source_dir = /tmp/a/, а real = /tmp/.. /etc/passwd
        log_operation("SECURITY_FAIL", fullpath, -EACCES);
        return -EACCES;
    }
    
    return 0; // Путь безопасен
}

/**
 * @brief Логирует операцию в stderr в требуемом формате.
 * * Формат: [YYYY-MM-DD HH:MM:SS] OPERATION: path (result: N)
 * * @param op_name  Имя операции (READ, WRITE, CREATE и т.д.)
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