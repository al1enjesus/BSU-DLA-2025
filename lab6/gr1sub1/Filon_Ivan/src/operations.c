#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>

static char base_root[PATH_MAX];

static void make_real_path(char *out, const char *in) {
    snprintf(out, PATH_MAX, "%s%s", base_root, in);
}

static int pt_getattr(const char *p, struct stat *st, struct fuse_file_info *ff) {
    (void) ff;

    char real[PATH_MAX];
    make_real_path(real, p);

    if (lstat(real, st) != 0)
        return -errno;

    return 0;
}

static int pt_readdir(const char *p, void *buf, fuse_fill_dir_t fill,
                      off_t off, struct fuse_file_info *fi, enum fuse_readdir_flags fl) {

    (void) off; (void) fi; (void) fl;

    char real[PATH_MAX];
    make_real_path(real, p);

    DIR *dp = opendir(real);
    if (!dp)
        return -errno;

    struct dirent *de;

    fill(buf, ".", NULL, 0, 0);
    fill(buf, "..", NULL, 0, 0);

    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));

        char full[PATH_MAX];
        snprintf(full, PATH_MAX, "%s/%s", real, de->d_name);

        stat(full, &st);

        if (fill(buf, de->d_name, &st, 0, 0))
            break;
    }

    closedir(dp);
    return 0;
}

static int pt_open(const char *p, struct fuse_file_info *fi) {
    char real[PATH_MAX];
    make_real_path(real, p);

    int fd = open(real, fi->flags);
    if (fd < 0)
        return -errno;

    close(fd);
    return 0;
}

static int pt_read(const char *p, char *buf, size_t sz, off_t off, struct fuse_file_info *fi) {
    (void) fi;
    char real[PATH_MAX];
    make_real_path(real, p);

    int fd = open(real, O_RDONLY);
    if (fd < 0)
        return -errno;
    ssize_t r = pread(fd, buf, sz, off);
    close(fd);

    return (r < 0) ? -errno : r;
}

static int pt_write(const char *p, const char *buf, size_t sz, off_t off, struct fuse_file_info *fi) {
    (void) fi;
    char real[PATH_MAX];
    make_real_path(real, p);
    int fd = open(real, O_WRONLY);
    if (fd < 0)
        return -errno;

    ssize_t w = pwrite(fd, buf, sz, off);
    close(fd);

    return (w < 0) ? -errno : w;
}

static int pt_mkdir(const char *p, mode_t m) {
    char real[PATH_MAX];
    make_real_path(real, p);

    if (mkdir(real, m) != 0)
        return -errno;

    return 0;
}

static int pt_rmdir(const char *p) {
    char real[PATH_MAX];
    make_real_path(real, p);
    if (rmdir(real) != 0)
        return -errno;

    return 0;
}

static int pt_unlink(const char *p) {
    char real[PATH_MAX];
    make_real_path(real, p);

    if (unlink(real) != 0)
        return -errno;

    return 0;
}

static int pt_rename(const char *oldp, const char *newp, unsigned int flags) {
    (void) flags;
    char r_old[PATH_MAX];
    char r_new[PATH_MAX];
    make_real_path(r_old, oldp);
    make_real_path(r_new, newp);

    if (rename(r_old, r_new) != 0)
        return -errno;

    return 0;
}

static struct fuse_operations pt_ops = {
    .getattr = pt_getattr,
    .readdir = pt_readdir,
    .open    = pt_open,
    .read    = pt_read,
    .write   = pt_write,
    .mkdir   = pt_mkdir,
    .rmdir   = pt_rmdir,
    .unlink  = pt_unlink,
    .rename  = pt_rename
};

int main(int argc, char *argv[]) {

    if (argc < 3) {
        fprintf(stderr, "Использование: %s <директория> <точка_монтирования>\n", argv[0]);
        return 1;
    }
    if (!realpath(argv[1], base_root)) {
        perror("realpath");
        return 1;
    }

    fprintf(stderr, "Проброс на директорию: %s\n", base_root);

    for (int i = 1; i < argc - 1; i++)
        argv[i] = argv[i + 1];
    argc--;
    return fuse_main(argc, argv, &pt_ops, NULL);
}
