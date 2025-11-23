#ifndef OPERATIONS_H
#define OPERATIONS_H

#define _GNU_SOURCE
#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>

// Глобальная переменная для базовой директории
extern char *base_path;

// Вспомогательные функции
void log_operation(const char *op, const char *path, int result);
void get_full_path(char *fullpath, const char *path);

// ROT13 функции для задания B
void rot13_transform(char *buf, size_t size);

// Uppercase функции для задания C
void uppercase_transform(char *buf, size_t size);

// FUSE операции для задания A (passthrough)
int passthrough_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi);
int passthrough_readdir(const char *path, void *buf, fuse_fill_dir_t filler, 
                       off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags);
int passthrough_open(const char *path, struct fuse_file_info *fi);
int passthrough_read(const char *path, char *buf, size_t size, off_t offset, 
                    struct fuse_file_info *fi);
int passthrough_write(const char *path, const char *buf, size_t size, off_t offset, 
                     struct fuse_file_info *fi);
int passthrough_create(const char *path, mode_t mode, struct fuse_file_info *fi);
int passthrough_unlink(const char *path);
int passthrough_mkdir(const char *path, mode_t mode);
int passthrough_rmdir(const char *path);

// ROT13 операции для задания B
int rot13_read(const char *path, char *buf, size_t size, off_t offset, 
              struct fuse_file_info *fi);
int rot13_write(const char *path, const char *buf, size_t size, off_t offset, 
               struct fuse_file_info *fi);

// Uppercase операции для задания C
int uppercase_read(const char *path, char *buf, size_t size, off_t offset, 
                  struct fuse_file_info *fi);

#endif // OPERATIONS_H