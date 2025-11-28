#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

static char *archive_path = NULL;

void log_op(const char *op, const char *path, int res) {
    time_t t = time(NULL);
    fprintf(stderr, "[%s] %s: %s -> %d\n", ctime(&t), op, path, res);
}

static int archive_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
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
    if (strcmp(path, "/") != 0) {
        log_op("ARCHIVE_READDIR", path, -ENOTDIR);
        return -ENOTDIR;
    }

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    
    // Фиктивные файлы в архиве
    filler(buf, "document.pdf", NULL, 0, 0);
    filler(buf, "image.jpg", NULL, 0, 0);
    filler(buf, "data.txt", NULL, 0, 0);
    filler(buf, "archive.tar", NULL, 0, 0);

    log_op("ARCHIVE_READDIR", path, 0);
    return 0;
}

static int archive_open(const char *path, struct fuse_file_info *fi) {
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
    char *content = "This is read-only archive file content.\n"
                   "File: ";
    char full_content[256];
    snprintf(full_content, sizeof(full_content), "%s%s\n", content, path + 1);
    
    size_t len = strlen(full_content);
    if (offset >= len) return 0;
    if (offset + size > len) size = len - offset;
    
    memcpy(buf, full_content + offset, size);
    log_op("ARCHIVE_READ", path, size);
    return size;
}

static int archive_write(const char *path, const char *buf, size_t size, off_t offset,
                        struct fuse_file_info *fi) {
    log_op("ARCHIVE_WRITE", path, -EROFS);
    return -EROFS; // Read-only
}

static int archive_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    log_op("ARCHIVE_CREATE", path, -EROFS);
    return -EROFS;
}

static int archive_unlink(const char *path) {
    log_op("ARCHIVE_UNLINK", path, -EROFS);
    return -EROFS;
}

static int archive_mkdir(const char *path, mode_t mode) {
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

    archive_path = realpath(argv[1], NULL);
    fprintf(stderr, "Mounting archive: %s\n", archive_path);

    argv[1] = argv[2];
    return fuse_main(argc - 1, argv + 1, &ops, NULL);
}