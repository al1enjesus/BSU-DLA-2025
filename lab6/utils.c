#include "utils.h"
#include <stdio.h>
#include <string.h>

/* Проверка на path traversal атаку */
static int is_path_safe(const char *path) {
    if (!path) return 0;
    
    /* Запрет путей с .. */
    if (strstr(path, "..") != NULL)
        return 0;
    
    return 1;
}

int build_full_path(char *buf, size_t bufsize, const char *root, const char *path) {
    if (!buf || !root || !path || bufsize == 0)
        return -1;
    
    /* Защита от path traversal */
    if (!is_path_safe(path))
        return -1;
    
    int written;
    if (strcmp(path, "/") == 0)
        written = snprintf(buf, bufsize, "%s", root);
    else
        written = snprintf(buf, bufsize, "%s%s", root, path);
    
    /* Проверка на переполнение буфера */
    if (written < 0 || (size_t)written >= bufsize)
        return -1;
    
    return 0;
}
