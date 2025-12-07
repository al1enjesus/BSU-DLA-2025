#include "../include/fuse_fs.h"

static int tar_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    if (is_stats_file(path)) {
        return stats_getattr(path, stbuf);
    }

    tar_entry_t *entry = tar_find(&g_tar, path);
    if (!entry) return -ENOENT;

    memset(stbuf, 0, sizeof(*stbuf));
    stbuf->st_mode = S_IFREG | (entry->mode & 0777);
    stbuf->st_nlink = 1;
    stbuf->st_size = entry->size;
    stbuf->st_mtime = entry->mtime;
    return 0;
}

static int tar_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
                       struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    if (strcmp(path, "/") != 0) return -ENOENT;
    return tar_list_root(&g_tar, buf, filler);
}

static int tar_open(const char *path, struct fuse_file_info *fi) {
    if ((fi->flags & O_WRONLY) || (fi->flags & O_RDWR)) {
        return -EACCES; // read-only
    }
    return 0;
}

static int tar_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    if (is_stats_file(path)) {
        return stats_read(path, buf, size, offset);
    }

    tar_entry_t *entry = tar_find(&g_tar, path);
    if (!entry) return -ENOENT;

    if (offset >= (off_t)entry->size) return 0;

    size_t avail = entry->size - offset;
    if (size > avail) size = avail;

    memcpy(buf, g_tar.data + entry->offset + offset, size);

    log_op("TAR_READ", path, (int)size);
    return (int)size;
}

struct fuse_operations tar_ops = {
    .getattr = tar_getattr,
    .readdir = tar_readdir,
    .open = tar_open,
    .read = tar_read,
};