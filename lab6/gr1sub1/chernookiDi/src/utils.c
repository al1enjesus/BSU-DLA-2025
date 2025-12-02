#include "operations.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>

/* Note: normalization is performed inside join_path; helper removed. */

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
    // Build safe path by joining resolved root and normalized components from `path`.
    // First, try to resolve realpath for g_config.root for symlink safety.
    char resolved_root[PATH_MAX_LEN];
    if (realpath(g_config.root, resolved_root) == NULL) {
        // fallback to rootcopy if realpath fails
        strncpy(resolved_root, rootcopy, PATH_MAX_LEN-1);
        resolved_root[PATH_MAX_LEN-1] = '\0';
    }
    // remove trailing slash from resolved_root if present (but keep "/" as-is)
    size_t rlen = strlen(resolved_root);
    if (rlen > 1 && resolved_root[rlen-1] == '/') {
        resolved_root[rlen-1] = '\0';
        rlen--;
    }

    // Prepare working buffer for the candidate path
    char candidate[PATH_MAX_LEN];
    // Start with resolved_root
    strncpy(candidate, resolved_root, PATH_MAX_LEN-1);
    candidate[PATH_MAX_LEN-1] = '\0';

    // If path is root ("/"), return resolved_root
    if (strcmp(path, "/") == 0) {
        strncpy(fullpath, candidate, PATH_MAX_LEN-1);
        fullpath[PATH_MAX_LEN-1] = '\0';
        return 0;
    }

    // Append components from path safely, resolving '.' and '..'
    const char *s = path;
    if (*s == '/') s++;
    while (*s) {
        // find next segment
        const char *slash = strchr(s, '/');
        size_t seglen = slash ? (size_t)(slash - s) : strlen(s);
        if (seglen == 0) {
            // skip
        } else if (seglen == 1 && s[0] == '.') {
            // skip
        } else if (seglen == 2 && s[0] == '.' && s[1] == '.') {
            // go up one level in candidate (but never above resolved_root)
            if (strlen(candidate) > rlen) {
                char *last = strrchr(candidate, '/');
                if (last && (size_t)(last - candidate) >= rlen) {
                    *last = '\0';
                } else {
                    // cannot go above root; keep candidate as root
                    candidate[rlen] = '\0';
                }
            }
        } else {
            // append '/segment' safely
            if (strlen(candidate) + 1 + seglen >= PATH_MAX_LEN) return -1;
            size_t off = strlen(candidate);
            candidate[off] = '/';
            memcpy(candidate + off + 1, s, seglen);
            candidate[off + 1 + seglen] = '\0';
        }
        if (!slash) break;
        s = slash + 1;
        while (*s == '/') s++;
    }

    // Final safety: ensure candidate begins with resolved_root
    if (strncmp(candidate, resolved_root, rlen) != 0) return -1;
    strncpy(fullpath, candidate, PATH_MAX_LEN-1);
    fullpath[PATH_MAX_LEN-1] = '\0';
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
