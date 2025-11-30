#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>

struct fs_stats {
    unsigned long reads;
    unsigned long writes;
    unsigned long opens;
    unsigned long bytes_read;
    unsigned long bytes_written;
};

static struct fs_stats stats = {0};
static const char *STATS_PATH = "/.stats";
static char *source_dir = NULL;

static void get_full_path(char *fpath, const char *path) {
    strcpy(fpath, source_dir);
    strcat(fpath, path);
}

static int do_getattr(const char *path, struct stat *st) {
    if (strcmp(path, STATS_PATH) == 0) {
        st->st_mode = S_IFREG | 0444;
        st->st_nlink = 1;
        st->st_size = 1024;
        return 0;
    }

    char fpath[1000];
    get_full_path(fpath, path);

    int res = lstat(fpath, st);
    if (res == -1)
        return -errno;

    return 0;
}

static int do_readdir(const char *path, void *buffer, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    char fpath[1000];
    get_full_path(fpath, path);

    DIR *dp = opendir(fpath);
    if (dp == NULL) return -errno;

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;

        if (filler(buffer, de->d_name, &st, 0, 0))
            break;
    }
    closedir(dp);
    return 0;
}

static int do_open(const char *path, struct fuse_file_info *fi) {
    stats.opens++;

    if (strcmp(path, STATS_PATH) == 0) {
        return 0;
    }

    char fpath[1000];
    get_full_path(fpath, path);

    int fd = open(fpath, fi->flags);
    if (fd == -1) return -errno;

    fi->fh = fd;
    return 0;
}

static int do_read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
    if (strcmp(path, STATS_PATH) == 0) {
        char stat_buf[1024];
        int len = sprintf(stat_buf, 
            "reads: %lu\n"
            "writes: %lu\n"
            "opens: %lu\n"
            "bytes_read: %lu\n"
            "bytes_written: %lu\n",
            stats.reads, stats.writes, stats.opens, 
            stats.bytes_read, stats.bytes_written);

        if (offset >= len) return 0;
        if (offset + size > len) size = len - offset;
        memcpy(buf, stat_buf + offset, size);
        
        return size;
    }

    stats.reads++;

    int res = pread(fi->fh, buf, size, offset);
    if (res == -1) return -errno;

    stats.bytes_read += res;
    return res;
}

static int do_write(const char *path, const char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi) {
    if (strcmp(path, STATS_PATH) == 0) {
        return -EPERM;
    }

    stats.writes++;

    int res = pwrite(fi->fh, buf, size, offset);
    if (res == -1) return -errno;

    stats.bytes_written += res;
    return res;
}

static int do_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char fpath[1000];
    get_full_path(fpath, path);

    int fd = creat(fpath, mode);
    if (fd == -1) return -errno;

    fi->fh = fd;
    return 0;
}

static int do_mkdir(const char *path, mode_t mode) {
    char fpath[1000];
    get_full_path(fpath, path);

    int res = mkdir(fpath, mode);
    if (res == -1) return -errno;

    return 0;
}

static int do_unlink(const char *path) {
    char fpath[1000];
    get_full_path(fpath, path);

    int res = unlink(fpath);
    if (res == -1) return -errno;

    return 0;
}

static int do_rmdir(const char *path) {
    char fpath[1000];
    get_full_path(fpath, path);

    int res = rmdir(fpath);
    if (res == -1) return -errno;

    return 0;
}

static struct fuse_operations operations = {
    .getattr = do_getattr,
    .readdir = do_readdir,
    .open    = do_open,
    .read    = do_read,
    .write   = do_write,
    .create  = do_create,
    .mkdir   = do_mkdir,
    .unlink  = do_unlink,
    .rmdir   = do_rmdir,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <source_dir> <mount_point>\n", argv[0]);
        return 1;
    }

    source_dir = realpath(argv[1], NULL);

    char *fuse_argv[] = { argv[0], argv[2], "-f", NULL };

    return fuse_main(3, fuse_argv, &operations, NULL);
}
