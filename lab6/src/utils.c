#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <sys/stat.h>
#include <unistd.h>

void log_timestamp(char *buf, size_t bufsz)
{
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(buf, bufsz, "%Y-%m-%d %H:%M:%S", &tm);
}

void log_operation(const char *op, const char *path, const char *result)
{
    char ts[64];
    log_timestamp(ts, sizeof(ts));
    fprintf(stderr, "[%s] %s: %s (%s)\n", ts, op, path, result);
    fflush(stderr);
}

char *build_fullpath(const char *source_dir, const char *path)
{
    if (!source_dir || !path) {
        errno = EINVAL;
        return NULL;
    }

    char tmp[PATH_MAX];
    if (strlen(source_dir) + strlen(path) + 2 > PATH_MAX) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    if (path[0] == '/')
        snprintf(tmp, sizeof(tmp), "%s%s", source_dir, path);
    else
        snprintf(tmp, sizeof(tmp), "%s/%s", source_dir, path);

    char *components[PATH_MAX/2];
    int compc = 0;

    char *s = tmp;
    while (*s == '/') ++s;
    while (*s) {
        char comp[PATH_MAX];
        int i = 0;
        while (*s && *s != '/') {
            comp[i++] = *s++;
        }
        comp[i] = '\0';
        if (strcmp(comp, "") == 0 || strcmp(comp, ".") == 0) {
        } else if (strcmp(comp, "..") == 0) {
            if (compc == 0) {
                errno = EACCES;
                return NULL;
            }
            free(components[--compc]);
        } else {
            components[compc] = strdup(comp);
            if (!components[compc]) {
                for (int j = 0; j < compc; ++j) free(components[j]);
                errno = ENOMEM;
                return NULL;
            }
            compc++;
        }
        while (*s == '/') ++s;
    }

    char *res = malloc(PATH_MAX);
    if (!res) {
        for (int j = 0; j < compc; ++j) free(components[j]);
        errno = ENOMEM;
        return NULL;
    }
    strcpy(res, source_dir);
    for (int i = 0; i < compc; ++i) {
        strcat(res, "/");
        strcat(res, components[i]);
        free(components[i]);
    }
    return res;
}