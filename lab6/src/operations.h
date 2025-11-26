// src/operations.h (Рабочая версия с Forward Declarations)
#ifndef OPERATIONS_H
#define OPERATIONS_H

// --- 1. Системные Типы (POSIX) ---
// Эти файлы определяют off_t, struct stat, mode_t, и т.д.
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

// #include <fuse.h> // <-- СТРОГО УДАЛЕНО!

// --- 2. Forward Declarations для FUSE (зависят от типов выше) ---
// Объявляем типы, которые используются в сигнатурах функций
typedef int (*fuse_fill_dir_t) (void *buf, const char *name, const struct stat *stbuf, off_t off);
struct fuse_file_info; // Для struct fuse_file_info *fi;

// Макрос для максимальной длины пути
#define PATH_MAX_LEN 1024

// --- Вспомогательные функции (из utils.c) ---
extern char *source_dir;
void get_full_path(char *fullpath, const char *path);
void log_operation(const char *op_name, const char *path, int result);

// --- Passthrough FUSE Operations (Сигнатуры FUSE 2.x) ---
int my_getattr(const char *path, struct stat *stbuf); 
int my_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi); 
int my_open(const char *path, struct fuse_file_info *fi);
int my_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int my_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int my_create(const char *path, mode_t mode, struct fuse_file_info *fi); 
int my_unlink(const char *path);
int my_mkdir(const char *path, mode_t mode);
int my_rmdir(const char *path);
int my_release(const char *path, struct fuse_file_info *fi);

#endif // OPERATIONS_H