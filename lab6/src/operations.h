// src/operations.h
#ifndef OPERATIONS_H
#define OPERATIONS_H

// --- 1. Системные Типы (POSIX) ---
// Эти файлы определяют off_t, size_t, struct stat, mode_t, и т.д.
#include <sys/types.h>  // <-- Определяет off_t (критично)
#include <sys/stat.h>
#include <unistd.h>     // <-- Может также определять off_t
#include <dirent.h>

// --- 2. Forward Declarations для FUSE ---

// struct stat уже определен выше
struct fuse_file_info; // Для struct fuse_file_info *fi; в сигнатурах

// typedef fuse_fill_dir_t зависит от off_t, поэтому off_t должен быть определен
// (Мы используем 4-аргументную сигнатуру, совместимую с FUSE 2/3)
typedef int (*fuse_fill_dir_t) (void *buf, const char *name, const struct stat *stbuf, off_t off);

// Макрос для максимальной длины пути
#define PATH_MAX_LEN 1024

// --- Вспомогательные функции (из utils.c) ---
extern char *source_dir;
void get_full_path(char *fullpath, const char *path);
void log_operation(const char *op_name, const char *path, int result);

// --- Passthrough FUSE Operations (из operations.c) ---
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