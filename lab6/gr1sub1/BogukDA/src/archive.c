#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "common.h"

static char *archive_path = NULL;

static int archive_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    if (!path || !stbuf) return -EINVAL;
    if (fi) 
    
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = 1024;
    }

    log_op("ARCHIVE_GETATTR", path, 0);
    return 0;
}

static int archive_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                          off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    if (!path || !buf || !filler) return -EINVAL;
    if (fi)
    
    if (strcmp(path, "/") != 0) {
        log_op("ARCHIVE_READDIR", path, -ENOTDIR);
        return -ENOTDIR;
    }

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    
    filler(buf, "document.pdf", NULL, 0, 0);
    filler(buf, "image.jpg", NULL, 0, 0);
    filler(buf, "data.txt", NULL, 0, 0);
    filler(buf, "archive.tar", NULL, 0, 0);

    log_op("ARCHIVE_READDIR", path, 0);
    return 0;
}

static int archive_open(const char *path, struct fuse_file_info *fi) {
    if (!path || !fi) return -EINVAL;
    
    if (strcmp(path, "/") == 0) {
        log_op("ARCHIVE_OPEN", path, -EISDIR);
        return -EISDIR;
    }

    if ((fi->flags & O_ACCMODE) != O_RDONLY) {
        log_op("ARCHIVE_OPEN", path, -EACCES);
        return -EACCES;
    }

    log_op("ARCHIVE_OPEN", path, 0);
    return 0;
}

static int archive_read(const char *path, char *buf, size_t size, off_t offset,
                       struct fuse_file_info *fi) {
    if (!path || !buf) return -EINVAL;
    if (fi) 
    
    char content[512];
    const char *filename = (path[0] == '/') ? path + 1 : path;
    int len = snprintf(content, sizeof(content), 
        "This is read-only archive file content.\n"
        "File: %s\n"
        "Archive: %s\n",
        filename, archive_path ? archive_path : "unknown");
    
    if (len < 0 || len >= (int)sizeof(content)) {
        len = sizeof(content) - 1;
    }
    
    if (offset >= len) return 0;
    if (offset + size > len) size = len - offset;
    
    memcpy(buf, content + offset, size);
    log_op("ARCHIVE_READ", path, size);
    return size;
}

static int archive_write(const char *path, const char *buf, size_t size, off_t offset,
                        struct fuse_file_info *fi) {
    if (!path || !buf) return -EINVAL;
    if (fi) 
    
    log_op("ARCHIVE_WRITE", path, -EROFS);
    return -EROFS;
}

static int archive_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    if (!path || !fi) return -EINVAL;
    log_op("ARCHIVE_CREATE", path, -EROFS);
    return -EROFS;
}

static int archive_unlink(const char *path) {
    if (!path) return -EINVAL;
    log_op("ARCHIVE_UNLINK", path, -EROFS);
    return -EROFS;
}

static int archive_mkdir(const char *path, mode_t mode) {
    if (!path) return -EINVAL;
    log_op("ARCHIVE_MKDIR", path, -EROFS);
    return -EROFS;
}

static struct fuse_operations ops = {
    .getattr = archive_getattr,
    .readdir = archive_readdir,
    .open = archive_open,
    .read = archive_read,
    .write = archive_write,
    .create = archive_create,
    .unlink = archive_unlink,
    .mkdir = archive_mkdir,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <archive_file> <mount_point> [FUSE options]\n", argv[0]);
        return 1;
    }

    char *temp_archive = realpath(argv[1], NULL);
    if (temp_archive) {
        fprintf(stderr, "Mounting archive: %s\n", temp_archive);
        archive_path = temp_archive;
    } else {
        fprintf(stderr, "Warning: Cannot resolve archive path, using default\n");
        archive_path = NULL;
    }

    argv[1] = argv[2];
    int ret = fuse_main(argc - 1, argv + 1, &ops, NULL);
    
    free(temp_archive);
    return ret;
}