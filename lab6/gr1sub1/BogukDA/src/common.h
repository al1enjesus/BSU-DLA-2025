#ifndef COMMON_H
#define COMMON_H

#include <errno.h>
#include <string.h>
#include <stdlib.h>

static inline char* get_full_path(const char *base_path, const char *path) {
    if (strstr(path, "..") != NULL) return NULL;
    
    size_t len = strlen(base_path) + strlen(path) + 1;
    char *fp = malloc(len);
    if (fp) snprintf(fp, len, "%s%s", base_path, path);
    return fp;
}

static inline void log_op(const char *op, const char *path, int res) {
    time_t t = time(NULL);
    fprintf(stderr, "[%s] %s: %s -> %d\n", ctime(&t), op, path, res);
}

#endif