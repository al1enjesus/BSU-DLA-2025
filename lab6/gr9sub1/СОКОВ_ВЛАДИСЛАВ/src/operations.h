#ifndef OPERATIONS_H
#define OPERATIONS_H

#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <sys/types.h>

/* Mode of filesystem behavior */
typedef enum {
    MODE_PASSTHROUGH = 0,
    MODE_ROT13,
    MODE_UPPER
} fs_mode_t;

/* Initialize module: set base path and mode */
int fs_init(const char *base, fs_mode_t mode);

/* FUSE callbacks (exported) */
int fs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi);
int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
               off_t offset, struct fuse_file_info *fi,
               enum fuse_readdir_flags flags);
int fs_open(const char *path, struct fuse_file_info *fi);
int fs_read(const char *path, char *buf, size_t size, off_t offset,
            struct fuse_file_info *fi);
int fs_write(const char *path, const char *buf, size_t size,
             off_t offset, struct fuse_file_info *fi);
int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi);
int fs_unlink(const char *path);
int fs_mkdir(const char *path, mode_t mode);
int fs_rmdir(const char *path);
void fs_destroy(void *private_data);

#endif /* OPERATIONS_H */
