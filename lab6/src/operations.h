#ifndef OPERATIONS_H
#define OPERATIONS_H

#include "utils.h"

char* get_full_path_or_error(const char *path, int *error_code);

int myfuse_getattr(const char *path, struct stat *stbuf);
int myfuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                   off_t offset, struct fuse_file_info *fi);
int myfuse_open(const char *path, struct fuse_file_info *fi);
int myfuse_read(const char *path, char *buf, size_t size, off_t offset,
                struct fuse_file_info *fi);
int myfuse_write(const char *path, const char *buf, size_t size, off_t offset,
                 struct fuse_file_info *fi);
int myfuse_create(const char *path, mode_t mode, struct fuse_file_info *fi);
int myfuse_unlink(const char *path);
int myfuse_mkdir(const char *path, mode_t mode);
int myfuse_rmdir(const char *path);

#endif