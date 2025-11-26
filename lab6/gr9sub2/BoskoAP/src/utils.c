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


if (snprintf(out, outlen, "%s%s", g_base_path, path) >= (int)outlen) return -ENAMETOOLONG;


char resolved[PATH_MAX];
if (allow_nonexistent) {
char *dup = strdup(out);
if (!dup) return -ENOMEM;
char *parent = dirname(dup);
if (!realpath(parent, resolved)) {
free(dup);
return -errno;
}
free(dup);
size_t base_len = strlen(g_base_path);
if (strncmp(resolved, g_base_path, base_len) != 0) return -EACCES;
return 0;
} else {
}