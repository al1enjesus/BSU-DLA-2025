#include "utils.h"
#include <libgen.h>
#include <unistd.h>

const char* get_timestamp() {
    static char timestamp[TIMESTAMP_BUFFER_SIZE];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(timestamp, TIMESTAMP_BUFFER_SIZE, "%Y-%m-%d %H:%M:%S", tm_info);
    return timestamp;
}

char* canonicalize_path(const char *path) {
    if (!path) {
        return NULL;
    }

    char *resolved = realpath(path, NULL);
    if (!resolved) {
        // Если realpath не сработал, создаем копию исходного пути
        char *fallback = strdup(path);
        if (!fallback) {
            return NULL;
        }
        return fallback;
    }
    return resolved;
}

char* build_fullpath_safe(const char *base, const char *path, int *error_code) {
    if (!base || !path) {
        *error_code = -EINVAL;
        return NULL;
    }

    // Канонизация базового пути
    char *canonical_base = canonicalize_path(base);
    if (!canonical_base) {
        *error_code = -ENOMEM;
        return NULL;
    }

    // Проверка на path traversal
    if (strstr(path, "..") != NULL) {
        free(canonical_base);
        *error_code = -EACCES;
        return NULL;
    }

    char full_path[PATH_BUFFER_SIZE];
    int written = snprintf(full_path, sizeof(full_path), "%s%s",
                          canonical_base, path);

    // Освобождаем временную память сразу после использования
    free(canonical_base);

    if (written < 0 || written >= (int)sizeof(full_path)) {
        *error_code = -ENAMETOOLONG;
        return NULL;
    }

    // Финальная канонизация полного пути
    char *result = canonicalize_path(full_path);
    if (!result) {
        *error_code = -ENOENT;
        return NULL;
    }

    // Проверка, что результат внутри базовой директории
    if (strncmp(result, base, strlen(base)) != 0) {
        free(result);
        *error_code = -EACCES;
        return NULL;
    }

    *error_code = 0;
    return result;
}