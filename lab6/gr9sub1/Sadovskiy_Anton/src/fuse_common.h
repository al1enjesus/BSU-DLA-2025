#ifndef FUSE_COMMON_H
#define FUSE_COMMON_H

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <limits.h>

// Безопасное построение полного пути с защитой от path traversal
static inline int get_full_path_safe(char *fullpath, const char *source_dir, const char *path)
{
  // Проверка на NULL
  if (!fullpath || !source_dir || !path)
  {
    return -EINVAL;
  }

  // Проверка на path traversal атаки
  if (strstr(path, "..") != NULL)
  {
    fprintf(stderr, "Security: Path traversal attempt detected: %s\n", path);
    return -EACCES;
  }

  // Безопасное построение пути с проверкой длины
  int written = snprintf(fullpath, PATH_MAX, "%s%s", source_dir, path);
  if (written >= PATH_MAX)
  {
    fprintf(stderr, "Error: Path too long (>= %d bytes)\n", PATH_MAX);
    return -ENAMETOOLONG;
  }

  // Нормализация пути и проверка, что результат внутри source_dir
  char *normalized = realpath(fullpath, NULL);
  if (normalized != NULL)
  {
    size_t source_len = strlen(source_dir);
    int is_inside = (strncmp(normalized, source_dir, source_len) == 0);

    if (!is_inside)
    {
      fprintf(stderr, "Security: Path escape attempt: %s -> %s\n",
              fullpath, normalized);
      free(normalized);
      return -EACCES;
    }

    // Копируем нормализованный путь обратно
    snprintf(fullpath, PATH_MAX, "%s", normalized);
    free(normalized);
  }
  // Если файл не существует, realpath вернет NULL - это нормально для create операций

  return 0;
}

// Безопасное логирование операций
static inline void log_operation_safe(const char *op, const char *path, int result)
{
  time_t now = time(NULL);
  char timestamp[64];
  struct tm *tm_info = localtime(&now);

  if (tm_info == NULL)
  {
    fprintf(stderr, "[UNKNOWN TIME] %s: %s (result: %d)\n", op, path, result);
    return;
  }

  if (strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info) == 0)
  {
    fprintf(stderr, "[TIME FORMAT ERROR] %s: %s (result: %d)\n", op, path, result);
    return;
  }

  fprintf(stderr, "[%s] %s: %s (result: %d)\n", timestamp, op, path, result);
}

// Проверка и нормализация исходной директории
static inline char* validate_source_dir(const char *path)
{
  if (path == NULL)
  {
    fprintf(stderr, "Error: Source directory path is NULL\n");
    return NULL;
  }

  char *abs_path = realpath(path, NULL);
  if (abs_path == NULL)
  {
    perror("Error: Cannot resolve source directory");
    return NULL;
  }

  // Проверка, что это действительно директория
  struct stat st;
  if (stat(abs_path, &st) != 0)
  {
    perror("Error: Cannot stat source directory");
    free(abs_path);
    return NULL;
  }

  if (!S_ISDIR(st.st_mode))
  {
    fprintf(stderr, "Error: %s is not a directory\n", abs_path);
    free(abs_path);
    return NULL;
  }

  // Добавляем trailing slash если его нет
  size_t len = strlen(abs_path);
  if (len > 0 && abs_path[len - 1] != '/')
  {
    char *with_slash = malloc(len + 2);
    if (with_slash == NULL)
    {
      perror("Error: Memory allocation failed");
      free(abs_path);
      return NULL;
    }
    snprintf(with_slash, len + 2, "%s/", abs_path);
    free(abs_path);
    abs_path = with_slash;
  }

  fprintf(stderr, "Validated source directory: %s\n", abs_path);
  return abs_path;
}

#endif // FUSE_COMMON_H
