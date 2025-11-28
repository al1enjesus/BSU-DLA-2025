#ifndef COMMON_H
#define COMMON_H

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <libgen.h>
#include <limits.h>
#include <sys/stat.h>

static inline char* get_full_path(const char *base_path, const char *path) {
    if (!base_path || !path) return NULL;
    
    if (strstr(path, "..") != NULL) {
        return NULL;
    }
    char resolved_base[PATH_MAX];
    if (realpath(base_path, resolved_base) == NULL) {
        return NULL;
    }
    char full_path[PATH_MAX];
    int len = snprintf(full_path, sizeof(full_path), "%s/%s", resolved_base, 
                      (path[0] == '/') ? path + 1 : path);
    if (len < 0 || len >= (int)sizeof(full_path)) {
        return NULL;
    }
    
    char resolved_path[PATH_MAX];
    if (realpath(full_path, resolved_path) == NULL) {
        char *path_copy = strdup(full_path);
        char *parent_dir = dirname(path_copy);
        
        char resolved_parent[PATH_MAX];
        int parent_ok = (realpath(parent_dir, resolved_parent) != NULL) &&
                       (strncmp(resolved_parent, resolved_base, strlen(resolved_base)) == 0);
        
        free(path_copy);
        if (!parent_ok) {
            return NULL;
        }
        return strdup(full_path);
    }
    
    if (strncmp(resolved_path, resolved_base, strlen(resolved_base)) != 0) {
        return NULL;
    }
    
    return strdup(resolved_path);
}

static inline void log_op(const char *op, const char *path, int res) {
    time_t t = time(NULL);
    char time_buf[20];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&t));
    fprintf(stderr, "[%s] %s: %s -> %d\n", time_buf, op, path, res);
}
