#define _XOPEN_SOURCE 700
#define FUSE_USE_VERSION 30
#include <fuse3/fuse.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "operations.h"

char *build_fullpath(const char *source_dir, const char *path);
void log_operation(const char *op, const char *path, const char *result);

int myfuse_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    (void) fi;
    const char *src = (const char*)fuse_get_context()->private_data;
    char *full = build_fullpath(src, path);
    if (!full) {
        log_operation("getattr", path, "forbidden or error");
        return -errno;
    }
    int res = lstat(full, stbuf);
    if (res == -1) {
        char errbuf[64];
        snprintf(errbuf, sizeof(errbuf), "%s", strerror(errno));
        log_operation("getattr", path, errbuf);
        free(full);
        return -errno;
    }
    log_operation("getattr", path, "ok");
    free(full);
    return 0;
}

int myfuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                   off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags)
{
    (void) offset; (void) fi; (void) flags;
    const char *src = (const char*)fuse_get_context()->private_data;
    char *full = build_fullpath(src, path);
    if (!full) {
        log_operation("readdir", path, "forbidden");
        return -EACCES;
    }
    DIR *dp = opendir(full);
    if (!dp) {
        char errbuf[64]; snprintf(errbuf, sizeof(errbuf), "%s", strerror(errno));
        log_operation("readdir", path, errbuf);
        free(full);
        return -errno;
    }
    struct dirent *de;
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    while ((de = readdir(dp)) != NULL) {
        filler(buf, de->d_name, NULL, 0, 0);
    }
    closedir(dp);
    log_operation("readdir", path, "ok");
    free(full);
    return 0;
}

int myfuse_open(const char *path, struct fuse_file_info *fi)
{
    const char *src = (const char*)fuse_get_context()->private_data;
    char *full = build_fullpath(src, path);
    if (!full) {
        log_operation("open", path, "forbidden");
        return -EACCES;
    }
    int fd = open(full, fi->flags);
    if (fd == -1) {
        char errbuf[64]; snprintf(errbuf, sizeof(errbuf), "%s", strerror(errno));
        log_operation("open", path, errbuf);
        free(full);
        return -errno;
    }
    fi->fh = fd;
    log_operation("open", path, "ok");
    free(full);
    return 0;
}

int myfuse_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    (void) path;
    int fd = fi->fh;
    ssize_t res = pread(fd, buf, size, offset);
    if (res == -1) {
        char errbuf[64]; snprintf(errbuf, sizeof(errbuf), "%s", strerror(errno));
        log_operation("read", path, errbuf);
        return -errno;
    }
    char okbuf[32]; snprintf(okbuf, sizeof(okbuf), "%zd bytes", res);
    log_operation("read", path, okbuf);
    return res;
}

int myfuse_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    (void) path;
    int fd = fi->fh;
    ssize_t res = pwrite(fd, buf, size, offset);
    if (res == -1) {
        char errbuf[64]; snprintf(errbuf, sizeof(errbuf), "%s", strerror(errno));
        log_operation("write", path, errbuf);
        return -errno;
    }
    char okbuf[32]; snprintf(okbuf, sizeof(okbuf), "%zd bytes", res);
    log_operation("write", path, okbuf);
    return res;
}

int myfuse_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    const char *src = (const char*)fuse_get_context()->private_data;
    char *full = build_fullpath(src, path);
    if (!full) {
        log_operation("create", path, "forbidden");
        return -EACCES;
    }
    int fd = open(full, fi->flags | O_CREAT, mode);
    if (fd == -1) {
        char errbuf[64]; snprintf(errbuf, sizeof(errbuf), "%s", strerror(errno));
        log_operation("create", path, errbuf);
        free(full);
        return -errno;
    }
    fi->fh = fd;
    log_operation("create", path, "ok");
    free(full);
    return 0;
}

int myfuse_unlink(const char *path)
{
    const char *src = (const char*)fuse_get_context()->private_data;
    char *full = build_fullpath(src, path);
    if (!full) {
        log_operation("unlink", path, "forbidden");
        return -EACCES;
    }
    int res = unlink(full);
    if (res == -1) {
        char errbuf[64]; snprintf(errbuf, sizeof(errbuf), "%s", strerror(errno));
        log_operation("unlink", path, errbuf);
        free(full);
        return -errno;
    }
    log_operation("unlink", path, "ok");
    free(full);
    return 0;
}

int myfuse_mkdir(const char *path, mode_t mode)
{
    const char *src = (const char*)fuse_get_context()->private_data;
    char *full = build_fullpath(src, path);
    if (!full) {
        log_operation("mkdir", path, "forbidden");
        return -EACCES;
    }
    int res = mkdir(full, mode);
    if (res == -1) {
        char errbuf[64]; snprintf(errbuf, sizeof(errbuf), "%s", strerror(errno));
        log_operation("mkdir", path, errbuf);
        free(full);
        return -errno;
    }
    log_operation("mkdir", path, "ok");
    free(full);
    return 0;
}

int myfuse_rmdir(const char *path)
{
    const char *src = (const char*)fuse_get_context()->private_data;
    char *full = build_fullpath(src, path);
    if (!full) {
        log_operation("rmdir", path, "forbidden");
        return -EACCES;
    }
    int res = rmdir(full);
    if (res == -1) {
        char errbuf[64]; snprintf(errbuf, sizeof(errbuf), "%s", strerror(errno));
        log_operation("rmdir", path, errbuf);
        free(full);
        return -errno;
    }
    log_operation("rmdir", path, "ok");
    free(full);
    return 0;
}

int myfuse_release(const char *path, struct fuse_file_info *fi)
{
    (void) path;
    if (fi->fh) close((int)fi->fh);
    fi->fh = 0;
    log_operation("release", path, "closed");
    return 0;
}

void fill_operations(struct fuse_operations *ops)
{
    ops->getattr = myfuse_getattr;
    ops->readdir = myfuse_readdir;
    ops->open = myfuse_open;
    ops->read = myfuse_read;
    ops->write = myfuse_write;
    ops->create = myfuse_create;
    ops->unlink = myfuse_unlink;
    ops->mkdir = myfuse_mkdir;
    ops->rmdir = myfuse_rmdir;
    ops->release = myfuse_release;
}