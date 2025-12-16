#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <stdarg.h>

static char *base_dir = NULL;

/* Собираем полный путь: base_dir + path */
static void get_full_path(char fpath[PATH_MAX], const char *path)
{
    snprintf(fpath, PATH_MAX, "%s%s", base_dir, path);
}

/* Печать лога с timestamp в stderr */
static void log_timestamped(const char *fmt, ...)
{
    time_t now = time(NULL);
    struct tm tm_now;
    char tbuf[64];

    localtime_r(&now, &tm_now);
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &tm_now);

    fprintf(stderr, "[%s] ", tbuf);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fprintf(stderr, "\n");
}

static void log_simple(const char *op, const char *path, int result)
{
    log_timestamped("%s: %s (result: %d)", op, path, result);
}

/* getattr */
static int myfs_getattr(const char *path, struct stat *stbuf)
{
    int res;
    char fpath[PATH_MAX];

    memset(stbuf, 0, sizeof(struct stat));
    get_full_path(fpath, path);

    res = lstat(fpath, stbuf);
    if (res == -1) {
        int err = -errno;
        log_simple("GETATTR", path, err);
        return err;
    }

    log_simple("GETATTR", path, 0);
    return 0;
}

/* readdir */
static int myfs_readdir(const char *path, void *buf,
                        fuse_fill_dir_t filler, off_t offset,
                        struct fuse_file_info *fi)
{
    DIR *dp;
    struct dirent *de;
    int res = 0;
    char fpath[PATH_MAX];

    (void) offset;
    (void) fi;

    get_full_path(fpath, path);

    dp = opendir(fpath);
    if (dp == NULL) {
        res = -errno;
        log_simple("READDIR", path, res);
        return res;
    }

    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;  /* DT_* -> S_IF* */

        if (filler(buf, de->d_name, &st, 0))
            break;
    }

    closedir(dp);
    log_simple("READDIR", path, 0);
    return 0;
}

/* open */
static int myfs_open(const char *path, struct fuse_file_info *fi)
{
    int fd;
    char fpath[PATH_MAX];

    get_full_path(fpath, path);

    fd = open(fpath, fi->flags);
    if (fd == -1) {
        int err = -errno;
        log_simple("OPEN", path, err);
        return err;
    }

    fi->fh = fd;
    log_simple("OPEN", path, 0);
    return 0;
}

/* release (закрытие файла) */
static int myfs_release(const char *path, struct fuse_file_info *fi)
{
    int res;

    res = close((int)fi->fh);
    if (res == -1) {
        int err = -errno;
        log_simple("RELEASE", path, err);
        return err;
    }

    log_simple("RELEASE", path, 0);
    return 0;
}

/* read */
static int myfs_read(const char *path, char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{
    int res;

    res = pread((int)fi->fh, buf, size, offset);
    if (res == -1) {
        int err = -errno;
        log_timestamped("READ: %s (%zu bytes at offset %lld, result: %d)",
                        path, size, (long long)offset, err);
        return err;
    }

    log_timestamped("READ: %s (%zu bytes at offset %lld, result: %d)",
                    path, size, (long long)offset, res);
    return res;
}

/* write */
static int myfs_write(const char *path, const char *buf, size_t size,
                      off_t offset, struct fuse_file_info *fi)
{
    int res;

    res = pwrite((int)fi->fh, buf, size, offset);
    if (res == -1) {
        int err = -errno;
        log_timestamped("WRITE: %s (%zu bytes at offset %lld, result: %d)",
                        path, size, (long long)offset, err);
        return err;
    }

    log_timestamped("WRITE: %s (%zu bytes at offset %lld, result: %d)",
                    path, size, (long long)offset, res);
    return res;
}

/* create */
static int myfs_create(const char *path, mode_t mode,
                       struct fuse_file_info *fi)
{
    int fd;
    char fpath[PATH_MAX];

    get_full_path(fpath, path);

    fd = open(fpath, fi->flags | O_CREAT, mode);
    if (fd == -1) {
        int err = -errno;
        log_simple("CREATE", path, err);
        return err;
    }

    fi->fh = fd;
    log_simple("CREATE", path, 0);
    return 0;
}

/* unlink */
static int myfs_unlink(const char *path)
{
    int res;
    char fpath[PATH_MAX];

    get_full_path(fpath, path);

    res = unlink(fpath);
    if (res == -1) {
        int err = -errno;
        log_simple("UNLINK", path, err);
        return err;
    }

    log_simple("UNLINK", path, 0);
    return 0;
}

/* mkdir */
static int myfs_mkdir(const char *path, mode_t mode)
{
    int res;
    char fpath[PATH_MAX];

    get_full_path(fpath, path);

    res = mkdir(fpath, mode);
    if (res == -1) {
        int err = -errno;
        log_simple("MKDIR", path, err);
        return err;
    }

    log_simple("MKDIR", path, 0);
    return 0;
}

/* rmdir */
static int myfs_rmdir(const char *path)
{
    int res;
    char fpath[PATH_MAX];

    get_full_path(fpath, path);

    res = rmdir(fpath);
    if (res == -1) {
        int err = -errno;
        log_simple("RMDIR", path, err);
        return err;
    }

    log_simple("RMDIR", path, 0);
    return 0;
}

static struct fuse_operations myfs_oper = {
    .getattr = myfs_getattr,
    .readdir = myfs_readdir,
    .open    = myfs_open,
    .release = myfs_release,
    .read    = myfs_read,
    .write   = myfs_write,
    .create  = myfs_create,
    .unlink  = myfs_unlink,
    .mkdir   = myfs_mkdir,
    .rmdir   = myfs_rmdir,
};

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr,
                "Usage: %s <source_dir> <mountpoint> [FUSE options]\n",
                argv[0]);
        return 1;
    }

    base_dir = realpath(argv[1], NULL);
    if (!base_dir) {
        perror("realpath");
        return 1;
    }

    /* Убираем source_dir из аргументов для fuse_main */
    int fuse_argc = argc - 1;
    char **fuse_argv = (char **)malloc(sizeof(char *) * fuse_argc);
    if (!fuse_argv) {
        perror("malloc");
        free(base_dir);
        return 1;
    }

    fuse_argv[0] = argv[0];
    for (int i = 2; i < argc; i++) {
        fuse_argv[i - 1] = argv[i];
    }

    int ret = fuse_main(fuse_argc, fuse_argv, &myfs_oper, NULL);

    free(fuse_argv);
    free(base_dir);
    return ret;
}