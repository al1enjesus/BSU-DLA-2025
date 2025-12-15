#include "utils.h"
#include <stdio.h>
#include <string.h>

void build_full_path(char *buf, const char *root, const char *path) {
    if (strcmp(path, "/") == 0)
        snprintf(buf, 4096, "%s", root);
    else
        snprintf(buf, 4096, "%s%s", root, path);
}
