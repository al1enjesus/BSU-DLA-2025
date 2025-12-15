#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

/* Возвращает 0 при успехе, -1 при ошибке (переполнение или path traversal) */
int build_full_path(char *buf, size_t bufsize, const char *root, const char *path);

#endif
