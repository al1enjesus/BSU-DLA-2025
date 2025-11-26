#ifndef UTILS_H
#define UTILS_H

#include <fuse3/fuse.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <time.h>

#define MAX_PATH_COMPONENTS 64
#define SAFE_STR_SIZE 256

// Безопасное построение полного пути
char* build_fullpath_safe(const char *base, const char *path, int *error_code);

// Проверка прав доступа
int check_access_permissions(const char *path, int mode);

// Валидация размера операций
int validate_operation_size(size_t size, off_t offset);

// Получение временной метки
const char* get_timestamp();

// Канонизация пути
char* canonicalize_path(const char *path);

#endif