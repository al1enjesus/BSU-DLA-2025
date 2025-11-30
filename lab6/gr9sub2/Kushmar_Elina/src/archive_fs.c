#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <errno.h>
#include <string.h>
#include "tar.h"

static char *tar_path;

static int fs_getattr(const char *path, struct stat *st, struct fuse_file_info *fi) {
    memset(st, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        st->st_mode = S_IFDIR | 0555;
        st->st_nlink = 2;
        return 0;
    }

    struct tar_entry *e = tar_find(path);
    if (!e) return -ENOENT;

    if (e->is_dir) {
        st->st_mode = S_IFDIR | 0555;
        st->st_nlink = 2;
    } else {
        st->st_mode = S_IFREG | 0444;
        st->st_size = e->size;
    }
    return 0;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags) {
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    return tar_list(path, buf, filler);
}

static int fs_open(const char *path, struct fuse_file_info *fi) {
    struct tar_entry *e = tar_find(path);
    if (!e) return -ENOENT;
    if (e->is_dir) return -EISDIR;
    return 0;
}

static int fs_read(const char *path, char *buf, size_t size,
                   off_t offset, struct fuse_file_info *fi) {
    return tar_read(path, buf, size, offset);
}

static struct fuse_operations ops = {
    .getattr = fs_getattr,
    .readdir = fs_readdir,
    .open = fs_open,
    .read = fs_read,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: archive_fs <archive.tar> <mountpoint>\n");
        return 1;
    }
    tar_path = argv[1];

    if (tar_load(tar_path) == -1) {
        fprintf(stderr, "failed to load tar\n");
        return 1;
    }

    return fuse_main(argc - 1, (char *[]){argv[0], argv[2], "-f"}, &ops, NULL);
}