#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

#define MAX_PATH_LEN 4096

/**
 * @brief Формирует полный физический путь на основе корневого каталога и логического пути FUSE.
 * @param buf Буфер для записи полного пути.
 * @param root Корневой физический путь.
 * @param path Логический путь, переданный FUSE (начинается с '/').
 */
void build_full_path(char *buf, const char *root, const char *path);

#endif // UTILS_H