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
#include <pthread.h>

static char *base_dir = NULL;

struct stats {
    unsigned long long reads;
    unsigned long long writes;
    unsigned long long opens;
    unsigned long long creates;
    unsigned long long unlinks;
    unsigned long long mkdirs;
    unsigned long long rmdirs;
    unsigned long long getattr;
    unsigned long long readdirs;
    unsigned long long bytes_read;
    unsigned long long bytes_written;
};

static struct stats g_stats;
static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

static void get_full_path(char fpath[PATH_MAX], const char *path)
{
    snprintf(fpath, PATH_MAX, "%s%s", base_dir, path);
}

static int is_stats_file(const char *path)
{
    return (strcmp(path, "/.stats") == 0);
}

/* Строим строку со статистикой, вызывающий обязан free() */
static char *build_stats_string(void)
{
    char *buf = (char *)malloc(1024);
    if (!buf)
        return NULL;

    pthread_mutex_lock(&stats_mutex);
    unsigned long long reads        = g_stats.reads;
    unsigned long long writes       = g_stats.writes;
    unsigned long long opens        = g_stats.opens;
    unsigned long long creates      = g_stats.creates;
    unsigned long long unlinks      = g_stats.unlinks;
    unsigned long long mkdirs       = g_stats.mkdirs;
    unsigned long long rmdirs       = g_stats.rmdirs;
    unsigned long long getattr      = g_stats.getattr;
    unsigned long long readdirs     = g_stats.readdirs;
    unsigned long long bytes_read   = g_stats.bytes_read;
    unsigned long long bytes_written= g_stats.bytes_written;
    pthread_mutex_unlock(&stats_mutex);

    snprintf(buf, 1024,
             "reads: %llu\n"
             "writes: %llu\n"
             "opens: %llu\n"
             "creates: %llu\n"
             "unlinks: %llu\n"
             "mkdirs: %llu\n"
             "rmdirs: %llu\n"
             "getattr: %llu\n"
             "readdirs: %llu\n"
             "bytes_read: %llu\n"
             "bytes_written: %llu\n",
             reads, writes, opens, creates, unlinks,
             mkdirs, rmdirs, getattr, readdirs,
             bytes_read, bytes_written);

    return buf;
}

/* getattr */
static int monfs_getattr(const char *path, struct stat *stbuf)
{
    memset(stbuf, 0, sizeof(struct stat));

    if (is_stats_file(path)) {
        char *s = build_stats_string();
        if (!s)
            return -ENOMEM;

        size_t len = strlen(s);
        free(s);

        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = len;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        return 0;
    }

    char fpath[PATH_MAX];
    get_full_path(fpath, path);

    int res = lstat(fpath, stbuf);

    pthread_mutex_lock(&stats_mutex);
    g_stats.getattr++;
    pthread_mutex_unlock(&stats_mutex);

    if (res == -1)
        return -errno;

    return 0;
}

/* readdir */
static int monfs_readdir(const char *path, void *buf,
                         fuse_fill_dir_t filler, off_t offset,
                         struct fuse_file_info *fi)
{
    (void)offset;
    (void)fi;

    DIR *dp;
    struct dirent *de;
    char fpath[PATH_MAX];

    if (strcmp(path, "/") == 0) {
        /* Добавляем . и .. и виртуальный файл .stats */
        filler(buf, ".", NULL, 0);
        filler(buf, "..", NULL, 0);
        filler(buf, ".stats", NULL, 0);

        get_full_path(fpath, path);
        dp = opendir(fpath);
        if (!dp)
            return -errno;

        while ((de = readdir(dp)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 ||
                strcmp(de->d_name, "..") == 0 ||
                strcmp(de->d_name, ".stats") == 0)
                continue;

            struct stat st;
            memset(&st, 0, sizeof(st));
            st.st_ino = de->d_ino;
            st.st_mode = de->d_type << 12;
            if (filler(buf, de->d_name, &st, 0))
                break;
        }

        closedir(dp);
    } else {
        get_full_path(fpath, path);
        dp = opendir(fpath);
        if (!dp)
            return -errno;

        while ((de = readdir(dp)) != NULL) {
            struct stat st;
            memset(&st, 0, sizeof(st));
            st.st_ino = de->d_ino;
            st.st_mode = de->d_type << 12;
            if (filler(buf, de->d_name, &st, 0))
                break;
        }

        closedir(dp);
    }

    pthread_mutex_lock(&stats_mutex);
    g_stats.readdirs++;
    pthread_mutex_unlock(&stats_mutex);

    return 0;
}

/* open */
static int monfs_open(const char *path, struct fuse_file_info *fi)
{
    if (is_stats_file(path)) {
        if ((fi->flags & O_ACCMODE) != O_RDONLY)
            return -EACCES;
        return 0;
    }

    char fpath[PATH_MAX];
    get_full_path(fpath, path);

    int fd = open(fpath, fi->flags);
    if (fd == -1)
        return -errno;

    fi->fh = fd;

    pthread_mutex_lock(&stats_mutex);
    g_stats.opens++;
    pthread_mutex_unlock(&stats_mutex);

    return 0;
}

/* release */
static int monfs_release(const char *path, struct fuse_file_info *fi)
{
    if (is_stats_file(path))
        return 0;

    int res = close((int)fi->fh);
    if (res == -1)
        return -errno;

    return 0;
}

/* read */
static int monfs_read(const char *path, char *buf, size_t size,
                      off_t offset, struct fuse_file_info *fi)
{
    if (is_stats_file(path)) {
        char *s = build_stats_string();
        if (!s)
            return -ENOMEM;

        size_t len = strlen(s);
        if ((size_t)offset >= len) {
            free(s);
            return 0;
        }

        if (offset + size > len)
            size = len - offset;

        memcpy(buf, s + offset, size);
        free(s);
        /* чтение .stats НЕ учитываем в статистике */
        return (int)size;
    }

    int res = pread((int)fi->fh, buf, size, offset);
    if (res == -1)
        return -errno;

    pthread_mutex_lock(&stats_mutex);
    g_stats.reads++;
    g_stats.bytes_read += (unsigned long long)res;
    pthread_mutex_unlock(&stats_mutex);

    return res;
}

/* write */
static int monfs_write(const char *path, const char *buf, size_t size,
                       off_t offset, struct fuse_file_info *fi)
{
    if (is_stats_file(path))
        return -EACCES;

    int res = pwrite((int)fi->fh, buf, size, offset);
    if (res == -1)
        return -errno;

    pthread_mutex_lock(&stats_mutex);
    g_stats.writes++;
    g_stats.bytes_written += (unsigned long long)res;
    pthread_mutex_unlock(&stats_mutex);

    return res;
}

/* create */
static int monfs_create(const char *path, mode_t mode,
                        struct fuse_file_info *fi)
{
    if (is_stats_file(path))
        return -EACCES;

    char fpath[PATH_MAX];
    get_full_path(fpath, path);

    int fd = open(fpath, fi->flags | O_CREAT, mode);
    if (fd == -1)
        return -errno;

    fi->fh = fd;

    pthread_mutex_lock(&stats_mutex);
    g_stats.creates++;
    pthread_mutex_unlock(&stats_mutex);

    return 0;
}

/* unlink */
static int monfs_unlink(const char *path)
{
    if (is_stats_file(path))
        return -EPERM;

    char fpath[PATH_MAX];
    get_full_path(fpath, path);

    int res = unlink(fpath);
    if (res == -1)
        return -errno;

    pthread_mutex_lock(&stats_mutex);
    g_stats.unlinks++;
    pthread_mutex_unlock(&stats_mutex);

    return 0;
}

/* mkdir */
static int monfs_mkdir(const char *path, mode_t mode)
{
    if (is_stats_file(path))
        return -EPERM;

    char fpath[PATH_MAX];
    get_full_path(fpath, path);

    int res = mkdir(fpath, mode);
    if (res == -1)
        return -errno;

    pthread_mutex_lock(&stats_mutex);
    g_stats.mkdirs++;
    pthread_mutex_unlock(&stats_mutex);

    return 0;
}

/* rmdir */
static int monfs_rmdir(const char *path)
{
    if (is_stats_file(path))
        return -EPERM;

    char fpath[PATH_MAX];
    get_full_path(fpath, path);

    int res = rmdir(fpath);
    if (res == -1)
        return -errno;

    pthread_mutex_lock(&stats_mutex);
    g_stats.rmdirs++;
    pthread_mutex_unlock(&stats_mutex);

    return 0;
}

static struct fuse_operations monfs_oper = {
    .getattr = monfs_getattr,
    .readdir = monfs_readdir,
    .open    = monfs_open,
    .release = monfs_release,
    .read    = monfs_read,
    .write   = monfs_write,
    .create  = monfs_create,
    .unlink  = monfs_unlink,
    .mkdir   = monfs_mkdir,
    .rmdir   = monfs_rmdir,
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

    int ret = fuse_main(fuse_argc, fuse_argv, &monfs_oper, NULL);

    free(fuse_argv);
    free(base_dir);
    return ret;
}