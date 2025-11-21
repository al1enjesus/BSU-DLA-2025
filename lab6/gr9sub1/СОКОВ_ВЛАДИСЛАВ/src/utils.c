/*
 * utils.c
 * Вспомогательные функции: логирование, безопасное составление пути,
 * ROT13/uppercase трансформации и т.д.
 */

#include "operations.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/limits.h>

extern char *g_base_path; /* определено в operations.c */
extern fs_mode_t g_mode;

/* Логирование операций в stderr: [TIMESTAMP] OP: path (result) */
void log_operation(const char *op, const char *path, int result) {
    time_t now = time(NULL);
    char ts[32];
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);
    fprintf(stderr, "[%s] %s: %s (result: %d)\n", ts, op, path, result);
}

/*
 * normalize_join:
 *  - безопасно соединяет base и rel (rel — путь, предоставляемый пользователем, начинается с '/')
 *  - убирает '..' и '.' компоненты без вызова realpath (работает даже для несуществующих путей)
 *  - записывает результат в out (должен быть PATH_MAX)
 *  - возвращает 0 при успехе, -EACCES если итог выходит за пределы base, или -ENOMEM/-EINVAL
 */
int normalize_join(const char *base, const char *rel, char *out, size_t out_sz) {
    if (!base || !rel || !out) return -EINVAL;

    /* ensure rel starts with '/' */
    const char *r = rel;
    while (*r == '/') r++; /* skip leading slashes */
    char temp[PATH_MAX];
    if (snprintf(temp, sizeof(temp), "%s/%s", base, r) >= (int)sizeof(temp))
        return -ENAMETOOLONG;

    /* Tokenize and resolve .. and . */
    char *tokens[PATH_MAX];
    int top = 0;
    char work[PATH_MAX];
    strncpy(work, temp, sizeof(work));
    work[sizeof(work)-1] = '\0';

    char *p = work;
    if (*p == '/') {
        p++;
    }

    char *saveptr = NULL;
    char *tok = strtok_r(p, "/", &saveptr);
    while (tok != NULL) {
        if (strcmp(tok, ".") == 0) {
            /* skip */
        } else if (strcmp(tok, "..") == 0) {
            if (top > 0) top--;
        } else {
            tokens[top++] = tok;
        }
        tok = strtok_r(NULL, "/", &saveptr);
    }

    /* rebuild path */
    char result[PATH_MAX];
    size_t pos = 0;
    /* start with base root slash if base had it */
    if (base[0] == '/') {
        result[pos++] = '/';
    }
    for (int i = 0; i < top; ++i) {
        size_t len = strlen(tokens[i]);
        if (pos + len + 1 >= sizeof(result)) return -ENAMETOOLONG;
        memcpy(result + pos, tokens[i], len);
        pos += len;
        if (i != top-1) result[pos++] = '/';
    }
    result[pos] = '\0';

    /* Now we must ensure that result starts with base (realpath of base) */
    char base_real[PATH_MAX];
    if (realpath(base, base_real) == NULL) {
        /* if base cannot be realpath-ed, fallback to base string */
        strncpy(base_real, base, sizeof(base_real));
        base_real[sizeof(base_real)-1] = '\0';
    }

    /* resolve real path of result's directory if possible */
    char result_cpy[PATH_MAX];
    if (result[0] == '/') {
        strncpy(result_cpy, result, sizeof(result_cpy));
        result_cpy[sizeof(result_cpy)-1] = '\0';
    }

    /* If result exists, use realpath to canonicalize; otherwise compare prefixes conservatively */
    char result_real[PATH_MAX];
    if (realpath(result_cpy, result_real) == NULL) {
        size_t blen = strlen(base_real);
        if (blen > 1 && base_real[blen-1] == '/') base_real[blen-1] = '\0';

        if (strncmp(result_cpy, base_real, strlen(base_real)) != 0) {
            return -EACCES;
        } else {
            if (strlen(result_cpy) + 1 > out_sz) return -ENAMETOOLONG;
            strncpy(out, result_cpy, out_sz);
            out[out_sz-1] = '\0';
            return 0;
        }
    } else {
        size_t blen = strlen(base_real);
        if (strncmp(result_real, base_real, blen) != 0) {
            return -EACCES;
        }
        if (strlen(result_real) + 1 > out_sz) return -ENAMETOOLONG;
        strncpy(out, result_real, out_sz);
        out[out_sz-1] = '\0';
        return 0;
    }
}

/* Apply ROT13 to a buffer in-place */
void rot13_transform_inplace(char *buf, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        char c = buf[i];
        if ('a' <= c && c <= 'z') {
            buf[i] = 'a' + ( (c - 'a' + 13) % 26 );
        } else if ('A' <= c && c <= 'Z') {
            buf[i] = 'A' + ( (c - 'A' + 13) % 26 );
        }
    }
}

/* Transform buffer to uppercase in-place (ASCII) */
void uppercase_inplace(char *buf, size_t size) {
    for (size_t i = 0; i < size; ++i) buf[i] = (char) toupper((unsigned char)buf[i]);
}
