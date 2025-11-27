#ifndef OPERATIONS_H
#define OPERATIONS_H

// --- Системные типы (POSIX) ---
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h> // Для PATH_MAX

// --- Forward Declarations для FUSE ---
typedef int (*fuse_fill_dir_t)(void *buf, const char *name, 
                                const struct stat *stbuf, off_t off);
struct fuse_file_info;

// Макрос для максимальной длины пути
#define PATH_MAX_LEN 4096

// --- Глобальная переменная ---
extern char *source_dir;

// --- Вспомогательные функции ---
// Возвращает 0 при успехе, -ENAMETOOLONG при переполнении
int get_full_path(char *fullpath, const char *path); 
// Проверяет путь на Path Traversal. Возвращает 0 или -EACCES.
int check_path_security(const char *fullpath); 
void log_operation(const char *op_name, const char *path, int result);

// --- FUSE Operations (FUSE 2.x совместимые) ---
int my_getattr(const char *path, struct stat *stbuf);
int my_readdir(const char *path, void *buf, fuse_fill_dir_t filler, 
               off_t offset, struct fuse_file_info *fi);
int my_open(const char *path, struct fuse_file_info *fi);
int my_read(const char *path, char *buf, size_t size, off_t offset, 
            struct fuse_file_info *fi);
int my_write(const char *path, const char *buf, size_t size, off_t offset, 
             struct fuse_file_info *fi);
int my_create(const char *path, mode_t mode, struct fuse_file_info *fi);
int my_unlink(const char *path);
int my_mkdir(const char *path, mode_t mode);
int my_rmdir(const char *path);
int my_release(const char *path, struct fuse_file_info *fi);

#endif // OPERATIONS_H