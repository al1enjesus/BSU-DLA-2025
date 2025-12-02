#include "operations.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>

/*
 * normalize_components
 * --------------------
 * Вспомогательная статическая функция.
 * Выполняет простую нормализацию пути, удаляя компоненты "." и корректно
 * обрабатывая ".." (поднимает уровень, если это возможно). Функция модифицирует
 * строку in-place: принимает строку в `out` и преобразует её в нормализованный путь.
 *
 * Примечание: это учебная реализация и она не покрывает всех граничных случаев
 * реальной файловой системы. Для production-решений рекомендуется использовать
 * `realpath` или тщательную проверку.
 */
static void normalize_components(char *out) {
    // simple normalization: remove ./ and handle ../ conservatively
    // This implementation removes occurrences of "/./" and resolves "/x/../" patterns.
    char tmp[PATH_MAX_LEN];
    char *parts[PATH_MAX_LEN];
    int pc = 0;
    strncpy(tmp, out, PATH_MAX_LEN-1);
    tmp[PATH_MAX_LEN-1] = 0;
    char *p = strtok(tmp, "/");
    while (p) {
        if (strcmp(p, "") == 0 || strcmp(p, ".") == 0) {
            // skip
        } else if (strcmp(p, "..") == 0) {
            if (pc > 0) pc--; // pop
        } else {
            parts[pc++] = p;
        }
        p = strtok(NULL, "/");
    }
    // rebuild out
    out[0] = '\0';
    if (pc == 0) {
        strcpy(out, "/");
        return;
    }
    for (int i = 0; i < pc; ++i) {
        strcat(out, "/");
        strcat(out, parts[i]);
    }
}

/*
 * join_path
 * ---------
 * Собирает абсолютный путь на основе `g_config.root` и относительного пути
 * `path` (путь из FUSE, начинающийся с '/'). Результат помещается в `fullpath`.
 * Возвращает 0 при успешной сборке и нормализации, иначе -1.
 *
 * Поведение:
 * - Убирает лишний слэш в конце у корневой директории `g_config.root`.
 * - Поддерживает `path == "/"` (тогда возвращает корневую директорию).
 * - Пытается нормализовать компоненты пути чтобы избежать простых атак через `..`.
 */
int join_path(char *fullpath, const char *path) {
    // path comes like "/file.txt"
    if (!path || !fullpath) return -1;
    size_t rootlen = strlen(g_config.root);
    if (rootlen + 1 + strlen(path) + 1 >= PATH_MAX_LEN) return -1;
    // ensure root doesn't end with '/'
    char rootcopy[PATH_MAX_LEN];
    strncpy(rootcopy, g_config.root, PATH_MAX_LEN-1);
    rootcopy[PATH_MAX_LEN-1] = 0;
    if (rootcopy[rootlen-1] == '/') rootcopy[rootlen-1] = '\0';
    // build
    if (strcmp(path, "/") == 0) {
        snprintf(fullpath, PATH_MAX_LEN, "%s/", rootcopy);
    } else {
        // remove leading '/'
        const char *p = path;
        if (p[0] == '/') p++;
        snprintf(fullpath, PATH_MAX_LEN, "%s/%s", rootcopy, p);
    }
    // normalize to prevent traversal
    char tmp[PATH_MAX_LEN];
    strncpy(tmp, fullpath, PATH_MAX_LEN-1);
    tmp[PATH_MAX_LEN-1] = 0;
    normalize_components(tmp);
    // If normalized path does not start with root, block (safety)
    if (strncmp(tmp, g_config.root, strlen(g_config.root)) != 0 && strcmp(g_config.root, "/") != 0) {
        // allow if root is '/'
        // else return error
        return -1;
    }
    strncpy(fullpath, tmp, PATH_MAX_LEN-1);
    fullpath[PATH_MAX_LEN-1] = 0;
    return 0;
}

/*
 * get_timestamp
 * -------------
 * Записывает текущую локальную временную метку в `buf` формата
 * YYYY-MM-DD HH:MM:SS. Размер буфера должен быть достаточным для строки.
 */
void get_timestamp(char *buf, size_t n) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(buf, n, "%Y-%m-%d %H:%M:%S", &tm);
}

/*
 * log_op
 * ------
 * Логирует операцию в stderr в формате:
 * [TIMESTAMP] OPERATION: path (result: <код>)
 *
 * Параметры:
 * - op: имя операции (строка), например "READ" или "WRITE".
 * - path: путь внутри FUSE (например "/file.txt").
 * - result: код результата (0 при успехе или отрицательный errno).
 */
void log_op(const char *op, const char *path, int result) {
    char ts[64];
    get_timestamp(ts, sizeof(ts));
    fprintf(stderr, "[%s] %s: %s (result: %d)\n", ts, op, path, result);
}

/*
 * rot13_buf
 * ---------
 * Применяет ROT13-шифрование к байтам буфера `buf` длины `len`.
 * Алгоритм работает только с ASCII буквами A-Z и a-z, остальные байты не трогает.
 * Функция модифицирует буфер на месте.
 */
void rot13_buf(char *buf, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        char c = buf[i];
        if ((c >= 'a' && c <= 'z')) {
            buf[i] = ((c - 'a' + 13) % 26) + 'a';
        } else if ((c >= 'A' && c <= 'Z')) {
            buf[i] = ((c - 'A' + 13) % 26) + 'A';
        }
    }
}

/*
 * strtoupper_buf
 * ----------------
 * Преобразует содержимое буфера в верхний регистр (in-place).
 */
void strtoupper_buf(char *buf, size_t len) {
    for (size_t i = 0; i < len; ++i) buf[i] = toupper((unsigned char)buf[i]);
}
