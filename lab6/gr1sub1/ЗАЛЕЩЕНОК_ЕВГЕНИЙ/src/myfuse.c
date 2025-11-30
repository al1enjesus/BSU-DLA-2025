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
#include <limits.h>

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

static void cleanup_resources(void) {
    if (source_dir) {
        free(source_dir);
        source_dir = NULL;
    }
}

static int is_safe_path(const char *path) {
    if (!path) return 0;
    if (strstr(path, "/../") || strcmp(path, "..") == 0 || 
        (strlen(path) >= 3 && strcmp(path + strlen(path) - 3, "/..") == 0)) {
        return 0;
    }
    return 1;
}

static int get_full_path(char *fpath, size_t max_len, const char *path) {
    if (!path || !source_dir) return -EFAULT;
    
    if (!is_safe_path(path)) {
        return -EACCES;
    }

    int written = snprintf(fpath, max_len, "%s%s", source_dir, path);
    if (written < 0 || (size_t)written >= max_len) {
        return -ENAMETOOLONG;
    }
    return 0;
}

static int do_getattr(const char *path, struct stat *st) {
    if (strcmp(path, STATS_PATH) == 0) {
        st->st_mode = S_IFREG | 0444;
        st->st_nlink = 1;
        st->st_size = 1024;
        return 0;
    }

    char fpath[PATH_MAX];
    int err = get_full_path(fpath, sizeof(fpath), path);
    if (err != 0) return err;

    int res = lstat(fpath, st);
    if (res == -1)
        return -errno;

    return 0;
}

static int do_readdir(const char *path, void *buffer, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    char fpath[PATH_MAX];
    int err = get_full_path(fpath, sizeof(fpath), path);
    if (err != 0) return err;

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

    char fpath[PATH_MAX];
    int err = get_full_path(fpath, sizeof(fpath), path);
    if (err != 0) return err;

    int fd = open(fpath, fi->flags);
    if (fd == -1) return -errno;

    fi->fh = fd;
    return 0;
}

static int do_release(const char *path, struct fuse_file_info *fi) {
    if (strcmp(path, STATS_PATH) == 0) {
        return 0;
    }
    close(fi->fh);
    return 0;
}

static int do_read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
    if (strcmp(path, STATS_PATH) == 0) {
        char stat_buf[1024];
        int len = snprintf(stat_buf, sizeof(stat_buf),
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
    char fpath[PATH_MAX];
    int err = get_full_path(fpath, sizeof(fpath), path);
    if (err != 0) return err;

    int fd = creat(fpath, mode);
    if (fd == -1) return -errno;
    fi->fh = fd;
    return 0;
}

static int do_mkdir(const char *path, mode_t mode) {
    char fpath[PATH_MAX];
    int err = get_full_path(fpath, sizeof(fpath), path);
    if (err != 0) return err;
    int res = mkdir(fpath, mode);
    if (res == -1) return -errno;
    return 0;
}

static int do_unlink(const char *path) {
    char fpath[PATH_MAX];
    int err = get_full_path(fpath, sizeof(fpath), path);
    if (err != 0) return err;
    int res = unlink(fpath);
    if (res == -1) return -errno;
    return 0;
}

static int do_rmdir(const char *path) {
    char fpath[PATH_MAX];
    int err = get_full_path(fpath, sizeof(fpath), path);
    if (err != 0) return err;
    int res = rmdir(fpath);
    if (res == -1) return -errno;
    return 0;
}

static struct fuse_operations operations = {
    .getattr = do_getattr,
    .readdir = do_readdir,
    .open    = do_open,
    .release = do_release,
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

    atexit(cleanup_resources);

    source_dir = realpath(argv[1], NULL);
    if (!source_dir) {
        perror("Error resolving source path");
        return 1;
    }

    char *fuse_argv[] = { argv[0], argv[2], "-f", NULL };
    return fuse_main(3, fuse_argv, &operations, NULL);
}
