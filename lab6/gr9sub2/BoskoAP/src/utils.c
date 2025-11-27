
#define _GNU_SOURCE
#include "operations.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <libgen.h>
#include <errno.h>

void log_operation(const char *op, const char *path, int result) {
time_t now = time(NULL);
char ts[64];
struct tm t;
localtime_r(&now, &t);
strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &t);
fprintf(stderr, "[%s] %s: %s (result: %d)\n", ts, op, path, result);
}


extern char *g_base_path;


int build_fullpath(const char *path, char *out, size_t outlen, int allow_nonexistent) {
    if (!g_base_path) return -EACCES;
    if (!path || !out) return -EINVAL;

    /* Special case: root path */
    if (strcmp(path, "/") == 0) {
        if (snprintf(out, outlen, "%s", g_base_path) >= (int)outlen) return -ENAMETOOLONG;
        return 0;
    }

    /* Build candidate path: base + path (path begins with '/') */
    if (snprintf(out, outlen, "%s%s", g_base_path, path) >= (int)outlen)
        return -ENAMETOOLONG;

    char resolved[PATH_MAX];

    if (allow_nonexistent) {
        /* If file may not exist yet — resolve parent directory to avoid path traversal */
        char *dup = strdup(out);
        if (!dup) return -ENOMEM;
        char *parent = dirname(dup); /* dirname may modify dup */
        if (!realpath(parent, resolved)) {
            int err = -errno;
            free(dup);
            return err;
        }
        free(dup);

        size_t base_len = strlen(g_base_path);
        if (strncmp(resolved, g_base_path, base_len) != 0 ||
            !(resolved[base_len] == '\0' || resolved[base_len] == '/')) {
            return -EACCES;
        }
        return 0;
    } else {
        /* For existing paths — resolve full path and check it stays inside base */
        if (!realpath(out, resolved)) {
            int err = -errno;
            return err;
        }

        size_t base_len = strlen(g_base_path);
        if (strncmp(resolved, g_base_path, base_len) != 0 ||
            !(resolved[base_len] == '\0' || resolved[base_len] == '/')) {
            return -EACCES;
        }

        /* Copy canonical path back to out (safe because resolved fits PATH_MAX and outlen checked earlier) */
        if (strlen(resolved) >= outlen) return -ENAMETOOLONG;
        strncpy(out, resolved, outlen);
        out[outlen-1] = '\0';
        return 0;
    }
}

