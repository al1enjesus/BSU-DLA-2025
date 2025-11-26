#include "utils.h"
#include <libgen.h>
#include <unistd.h>

const char* get_timestamp() {
    static char timestamp[SAFE_STR_SIZE];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(timestamp, SAFE_STR_SIZE, "%Y-%m-%d %H:%M:%S", tm_info);
    return timestamp;
}

char* canonicalize_path(const char *path) {
    if (!path) return NULL;

    char *resolved = realpath(path, NULL);
    if (!resolved) {
        return strdup(path); // fallback
    }
    return resolved;
}

char* build_fullpath_safe(const char *base, const char *path, int *error_code) {
    if (!base || !path) {
        *error_code = -EINVAL;
        return NULL;
    }

    char *canonical_base = canonicalize_path(base);
    if (!canonical_base) {
        *error_code = -ENOMEM;
        return NULL;
    }

    if (strstr(path, "..") != NULL || strstr(path, "//") != NULL) {
        free(canonical_base);
        *error_code = -EACCES;
        return NULL;
    }

    char full_path[PATH_MAX];
    int written = snprintf(full_path, sizeof(full_path), "%s%s",
                          canonical_base, path);
    free(canonical_base);

    if (written < 0 || written >= (int)sizeof(full_path)) {
        *error_code = -ENAMETOOLONG;
        return NULL;
    }

    char *result = canonicalize_path(full_path);
    if (!result) {
        *error_code = -ENOENT;
        return NULL;
    }

    if (strncmp(result, base, strlen(base)) != 0) {
        free(result);
        *error_code = -EACCES;
        return NULL;
    }

    *error_code = 0;
    return result;
}

int check_access_permissions(const char *path, int mode) {
    return 0;
}

int validate_operation_size(size_t size, off_t offset) {
    if (size > SSIZE_MAX || offset < 0) {
        return -EINVAL;
    }
    return 0;
}