#define FUSE_USE_VERSION 30

#include <fuse3/fuse.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

#include "operations.h"

extern char base_path[PATH_MAX];
extern int mode_rot13;
extern int mode_upper;
extern int mode_passthrough;

/* ---------- SAFE FULLPATH ---------- */
static int fullpath(char out[PATH_MAX], const char *path) {
    if (snprintf(out, PATH_MAX, "%s%s", base_path, path) >= PATH_MAX)
        return -ENAMETOOLONG;
    return 0;
}

/* ---------- GETATTR ---------- */
static int fs_getattr(const char *path, struct stat *st, struct fuse_file_info *fi) {
    (void) fi;

    char real[PATH_MAX];
    int rc = fullpath(real, path);
    if (rc < 0) return rc;

    if (lstat(real, st) == -1)
        return -errno;

    return 0;
}

/* ---------- READDIR ---------- */
static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    char real[PATH_MAX];
    int rc = fullpath(real, path);
    if (rc < 0) return rc;

    DIR *dp = opendir(real);
    if (!dp) return -errno;

    struct dirent *de;
    while ((de = readdir(dp)) != NULL)
        filler(buf, de->d_name, NULL, 0, 0);

    closedir(dp);
    return 0;
}

/* ---------- OPEN ---------- */
static int fs_open(const char *path, struct fuse_file_info *fi) {
    char real[PATH_MAX];
    int rc = fullpath(real, path);
    if (rc < 0) return rc;

    int fd = open(real, fi->flags);
    if (fd == -1) return -errno;

    fi->fh = fd;
    return 0;
}

/* ---------- READ ---------- */
static int fs_read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
    (void) path;

    int res = pread(fi->fh, buf, size, offset);
    if (res < 0) return -errno;

    if (mode_rot13)
        rot13(buf, res);

    if (mode_upper)
        to_uppercase(buf, res);

    return res;
}

/* ---------- WRITE ---------- */
static int fs_write(const char *path, const char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi) {
    (void) path;

    char *tmp = malloc(size);
    if (!tmp) return -ENOMEM;

    memcpy(tmp, buf, size);

    if (mode_rot13)
        rot13(tmp, size);

    int res = pwrite(fi->fh, tmp, size, offset);
    free(tmp);

    if (res < 0) return -errno;

    return res;
}

/* ---------- CREATE ---------- */
static int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char real[PATH_MAX];
    int rc = fullpath(real, path);
    if (rc < 0) return rc;

    int fd = open(real, O_CREAT | O_WRONLY, mode);
    if (fd == -1) return -errno;

    fi->fh = fd;
    return 0;
}

/* ---------- UNLINK ---------- */
static int fs_unlink(const char *path) {
    char real[PATH_MAX];
    int rc = fullpath(real, path);
    if (rc < 0) return rc;

    if (unlink(real) == -1)
        return -errno;

    return 0;
}

/* ---------- MKDIR ---------- */
static int fs_mkdir(const char *path, mode_t mode) {
    char real[PATH_MAX];
    int rc = fullpath(real, path);
    if (rc < 0) return rc;

    if (mkdir(real, mode) == -1)
        return -errno;

    return 0;
}

/* ---------- RMDIR ---------- */
static int fs_rmdir(const char *path) {
    char real[PATH_MAX];
    int rc = fullpath(real, path);
    if (rc < 0) return rc;

    if (rmdir(real) == -1)
        return -errno;

    return 0;
}

/* ---------- OPERATIONS TABLE ---------- */
struct fuse_operations ops = {
    .getattr = fs_getattr,
    .readdir = fs_readdir,
    .open    = fs_open,
    .read    = fs_read,
    .write   = fs_write,
    .create  = fs_create,
    .unlink  = fs_unlink,
    .mkdir   = fs_mkdir,
    .rmdir   = fs_rmdir,
};
