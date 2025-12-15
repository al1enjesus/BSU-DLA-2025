#define _GNU_SOURCE
#define FUSE_USE_VERSION 35
#include "passthrough.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

static const char *root_dir;

static void log_op(const char *op, const char *path, int res) {
    fprintf(stderr, "[%ld] %s: %s (res=%d)\n", time(NULL), op, path, res);
}

/* ---------------- getattr ---------------- */
static int pt_getattr(const char *path, struct stat *st, struct fuse_file_info *fi)
{
    (void) fi;

    char full[4096];
    if (build_full_path(full, sizeof(full), root_dir, path) != 0) {
        log_op("GETATTR", path, -EINVAL);
        return -EINVAL;
    }

    int res = lstat(full, st);
    log_op("GETATTR", path, res == -1 ? -errno : 0);
    return res == -1 ? -errno : 0;
}

/* ---------------- readdir ---------------- */
static int pt_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
            off_t offset, struct fuse_file_info *fi,
            enum fuse_readdir_flags flags)
{
    char full[4096];
    if (build_full_path(full, sizeof(full), root_dir, path) != 0) {
        log_op("READDIR", path, -EINVAL);
        return -EINVAL;
    }

    DIR *dp = opendir(full);
    if (!dp) {
        log_op("READDIR", path, -errno);
        return -errno;
    }

    struct dirent *de;
    while ((de = readdir(dp))) {
        filler(buf, de->d_name, NULL, 0, 0);
    }

    closedir(dp);
    log_op("READDIR", path, 0);
    return 0;
}

/* ---------------- open ---------------- */
static int pt_open(const char *path, struct fuse_file_info *fi) {
    char full[4096];
    if (build_full_path(full, sizeof(full), root_dir, path) != 0) {
        log_op("OPEN", path, -EINVAL);
        return -EINVAL;
    }

    int fd = open(full, fi->flags);
    if (fd == -1) {
        log_op("OPEN", path, -errno);
        return -errno;
    }

    fi->fh = fd;
    log_op("OPEN", path, 0);
    return 0;
}

/* ---------------- read ---------------- */
static int pt_read(const char *path, char *buf, size_t size,
                   off_t offset, struct fuse_file_info *fi)
{
    int res = pread(fi->fh, buf, size, offset);
    if (res == -1) {
        log_op("READ", path, -errno);
        return -errno;
    }
    log_op("READ", path, res);
    return res;
}

/* ---------------- write ---------------- */
static int pt_write(const char *path, const char *buf, size_t size,
                   off_t offset, struct fuse_file_info *fi)
{
    int res = pwrite(fi->fh, buf, size, offset);
    if (res == -1) {
        log_op("WRITE", path, -errno);
        return -errno;
    }
    log_op("WRITE", path, res);
    return res;
}

/* ---------------- create ---------------- */
static int pt_create(const char *path, mode_t mode,
                     struct fuse_file_info *fi)
{
    char full[4096];
    if (build_full_path(full, sizeof(full), root_dir, path) != 0) {
        log_op("CREATE", path, -EINVAL);
        return -EINVAL;
    }

    int fd = open(full, fi->flags | O_CREAT, mode);
    if (fd == -1) {
        log_op("CREATE", path, -errno);
        return -errno;
    }
    fi->fh = fd;
    log_op("CREATE", path, 0);
    return 0;
}

/* ---------------- unlink ---------------- */
static int pt_unlink(const char *path) {
    char full[4096];
    if (build_full_path(full, sizeof(full), root_dir, path) != 0) {
        log_op("UNLINK", path, -EINVAL);
        return -EINVAL;
    }

    int res = unlink(full);
    log_op("UNLINK", path, res == -1 ? -errno : 0);
    return res == -1 ? -errno : 0;
}

/* ---------------- mkdir ---------------- */
static int pt_mkdir(const char *path, mode_t mode) {
    char full[4096];
    if (build_full_path(full, sizeof(full), root_dir, path) != 0) {
        log_op("MKDIR", path, -EINVAL);
        return -EINVAL;
    }

    int res = mkdir(full, mode);
    log_op("MKDIR", path, res == -1 ? -errno : 0);
    return res == -1 ? -errno : 0;
}

/* ---------------- rmdir ---------------- */
static int pt_rmdir(const char *path) {
    char full[4096];
    if (build_full_path(full, sizeof(full), root_dir, path) != 0) {
        log_op("RMDIR", path, -EINVAL);
        return -EINVAL;
    }

    int res = rmdir(full);
    log_op("RMDIR", path, res == -1 ? -errno : 0);
    return res == -1 ? -errno : 0;
}

static struct fuse_operations pt_ops;

struct fuse_operations *get_passthrough_ops(const char *root) {
    root_dir = root;

    memset(&pt_ops, 0, sizeof(pt_ops));
    pt_ops.getattr = pt_getattr;
    pt_ops.readdir = pt_readdir;
    pt_ops.open    = pt_open;
    pt_ops.read    = pt_read;
    pt_ops.write   = pt_write;
    pt_ops.create  = pt_create;
    pt_ops.unlink  = pt_unlink;
    pt_ops.mkdir   = pt_mkdir;
    pt_ops.rmdir   = pt_rmdir;

    return &pt_ops;
}
