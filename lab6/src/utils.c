#include "utils.h"
#include <stdio.h>
#include <string.h>

void build_full_path(char *buf, const char *root, const char *path) {
    // Если путь - это просто корень
    if (strcmp(path, "/") == 0)
        snprintf(buf, MAX_PATH_LEN, "%s", root);
    // Иначе, объединяем root и path (path уже имеет ведущий '/')
    else
        snprintf(buf, MAX_PATH_LEN, "%s%s", root, path);
}