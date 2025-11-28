#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include "common.h"

static char *base_path = NULL;

static struct {
    int reads, writes, opens, getattrs, readdirs;
    size_t bytes_read, bytes_written;
} stats = {0};

static inline void update_stats(const char *op, int bytes) {
    if (strcmp(op, "READ") == 0) { stats.reads++; stats.bytes_read += bytes; }
    else if (strcmp(op, "WRITE") == 0) { stats.writes++; stats.bytes_written += bytes; }
    else if (strcmp(op, "OPEN") == 0) stats.opens++;
    else if (strcmp(op, "GETATTR") == 0) stats.getattrs++;
    else if (strcmp(op, "READDIR") == 0) stats.readdirs++;
}

static int monitor_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    if (!path || !stbuf) return -EINVAL;
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/.stats") == 0) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = 256;
        log_op("GETATTR", path, 0);
        update_stats("GETATTR", 0);
        return 0;
    }

    char *fp = get_full_path(base_path, path);
    if (!fp) return -ENOENT;
    
    int res = lstat(fp, stbuf);
    if (res == -1) res = -errno;
    
    log_op("GETATTR", path, res);
    update_stats("GETATTR", 0);
    free(fp);
    return res;
}

static int monitor_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                          off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    if (!path || !buf || !filler) return -EINVAL;
    
    char *fp = get_full_path(base_path, path);
    if (!fp) return -ENOENT;
    
    DIR *dp = opendir(fp);
    if (!dp) {
        int res = -errno;
        log_op("READDIR", path, res);
        free(fp);
        return res;
    }

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0, 0)) break;
    }

    if (strcmp(path, "/") == 0) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_mode = S_IFREG | 0444;
        st.st_size = 256;
        filler(buf, ".stats", &st, 0, 0);
    }

    closedir(dp);
    log_op("READDIR", path, 0);
    update_stats("READDIR", 0);
    free(fp);
    return 0;
}

static int monitor_open(const char *path, struct fuse_file_info *fi) {
    if (!path || !fi) return -EINVAL;
    
    if (strcmp(path, "/.stats") == 0) {
        if ((fi->flags & O_ACCMODE) != O_RDONLY) return -EACCES;
        log_op("OPEN", path, 0);
        update_stats("OPEN", 0);
        return 0;
    }

    char *fp = get_full_path(base_path, path);
    if (!fp) return -ENOENT;
    
    int fd = open(fp, fi->flags);
    int res = (fd == -1) ? -errno : 0;
    if (fd != -1) close(fd);
    
    log_op("OPEN", path, res);
    update_stats("OPEN", 0);
    free(fp);
    return res;
}

static int monitor_read(const char *path, char *buf, size_t size, off_t offset,
                       struct fuse_file_info *fi) {
    if (!path || !buf) return -EINVAL;
    
    if (strcmp(path, "/.stats") == 0) {
        char stat_buf[512];
        int len = snprintf(stat_buf, sizeof(stat_buf),
            "File System Statistics:\n"
            "reads: %d\nwrites: %d\nopens: %d\n"
            "getattrs: %d\nreaddirs: %d\n"
            "bytes_read: %zu\nbytes_written: %zu\n",
            stats.reads, stats.writes, stats.opens,
            stats.getattrs, stats.readdirs,
            stats.bytes_read, stats.bytes_written);

        if (offset >= len) return 0;
        if (offset + size > len) size = len - offset;
        
        memcpy(buf, stat_buf + offset, size);
        log_op("READ", path, size);
        update_stats("READ", size);
        return size;
    }

    char *fp = get_full_path(base_path, path);
    if (!fp) return -ENOENT;
    
    int fd = open(fp, O_RDONLY);
    if (fd == -1) {
        int res = -errno;
        log_op("READ", path, res);
        free(fp);
        return res;
    }

    int res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;
    
    close(fd);
    log_op("READ", path, res);
    update_stats("READ", res > 0 ? res : 0);
    free(fp);
    return res;
}

static int monitor_write(const char *path, const char *buf, size_t size, off_t offset,
                        struct fuse_file_info *fi) {
    if (!path || !buf) return -EINVAL;
    
    if (strcmp(path, "/.stats") == 0) {
        log_op("WRITE", path, -EACCES);
        update_stats("WRITE", 0);
        return -EACCES;
    }

    char *fp = get_full_path(base_path, path);
    if (!fp) return -ENOENT;
    
    int fd = open(fp, O_WRONLY);
    if (fd == -1) {
        int res = -errno;
        log_op("WRITE", path, res);
        free(fp);
        return res;
    }

    int res = pwrite(fd, buf, size, offset);
    if (res == -1) res = -errno;
    
    close(fd);
    log_op("WRITE", path, res);
    update_stats("WRITE", res > 0 ? res : 0);
    free(fp);
    return res;
}

static struct fuse_operations ops = {
    .getattr = monitor_getattr,
    .readdir = monitor_readdir,
    .open = monitor_open,
    .read = monitor_read,
    .write = monitor_write,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mount_point> [FUSE options]\n", argv[0]);
        return 1;
    }

    base_path = realpath(argv[1], NULL);
    if (!base_path) {
        fprintf(stderr, "Error: Invalid source directory '%s'\n", argv[1]);
        return 1;
    }

    struct stat st;
    if (stat(base_path, &st) == -1 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: Cannot access source directory '%s'\n", base_path);
        free(base_path);
        return 1;
    }

    argv[1] = argv[2];
    int ret = fuse_main(argc - 1, argv + 1, &ops, NULL);
    
    free(base_path);
    return ret;
}