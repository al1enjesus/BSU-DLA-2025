#define _GNU_SOURCE
#define FUSE_USE_VERSION 35
#include "monitoring_fs.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>

static const char *root_dir;

static long count_open = 0;
static long count_read = 0;
static long count_write = 0;
static long count_getattr = 0;
static long bytes_read = 0;
static long bytes_written = 0;

static int mf_getattr(const char *path, struct stat *st, struct fuse_file_info *fi)
{
    (void)fi;
    count_getattr++;

    if (strcmp(path, "/.stats") == 0) {
        memset(st, 0, sizeof(*st));
        st->st_mode = S_IFREG | 0444;
        st->st_size = 1024;
        return 0;
    }

    char full[4096];
    build_full_path(full, root_dir, path);

    int r = lstat(full, st);
    return r == -1 ? -errno : 0;
}

static int mf_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t o, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags)
{
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    filler(buf, ".stats", NULL, 0, 0);

    char full[4096];
    build_full_path(full, root_dir, path);

    DIR *dp = opendir(full);
    if (!dp) return -errno;

    struct dirent *de;
    while ((de = readdir(dp))) {
        filler(buf, de->d_name, NULL, 0, 0);
    }
    closedir(dp);
    return 0;
}

static int mf_open(const char *path, struct fuse_file_info *fi) {
    if (strcmp(path, "/.stats") == 0)
        return 0;

    count_open++;

    char full[4096];
    build_full_path(full, root_dir, path);

    int fd = open(full, fi->flags);
    if (fd == -1) return -errno;
    fi->fh = fd;
    return 0;
}

static int mf_read(const char *path, char *buf, size_t size, off_t off,
                   struct fuse_file_info *fi)
{
    if (strcmp(path, "/.stats") == 0) {
        int n = snprintf(buf, size,
            "open: %ld\nread: %ld\nwrite: %ld\nbytes_read: %ld\nbytes_written: %ld\n",
            count_open, count_read, count_write, bytes_read, bytes_written);
        return n;
    }

    count_read++;

    int r = pread(fi->fh, buf, size, off);
    if (r > 0) bytes_read += r;
    return r;
}

static int mf_write(const char *path, const char *buf, size_t size,
                    off_t off, struct fuse_file_info *fi)
{
    count_write++;

    int r = pwrite(fi->fh, buf, size, off);
    if (r > 0) bytes_written += r;
    return r;
}

static struct fuse_operations ops;

struct fuse_operations *get_monitor_ops(const char *root) {
    root_dir = root;

    memset(&ops, 0, sizeof(ops));
    ops.getattr = mf_getattr;
    ops.readdir = mf_readdir;
    ops.open    = mf_open;
    ops.read    = mf_read;
    ops.write   = mf_write;

    return &ops;
}
