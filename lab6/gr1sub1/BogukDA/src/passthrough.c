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
#include <errno.h>

static char *base_path = NULL;

char* get_full_path(const char *path) {
    char *fp = malloc(strlen(base_path) + strlen(path) + 1);
    sprintf(fp, "%s%s", base_path, path);
    return fp;
}

void log_op(const char *op, const char *path, int res) {
    time_t t = time(NULL);
    fprintf(stderr, "[%s] %s: %s -> %d\n", ctime(&t), op, path, res);
}

static int passthrough_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    char *fp = get_full_path(path);
    int res = lstat(fp, stbuf);
    if (res == -1) res = -errno;
    log_op("GETATTR", path, res);
    free(fp);
    return res;
}

static int passthrough_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                              off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    char *fp = get_full_path(path);
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
        filler(buf, de->d_name, &st, 0, 0);
    }

    closedir(dp);
    log_op("READDIR", path, 0);
    free(fp);
    return 0;
}

static int passthrough_open(const char *path, struct fuse_file_info *fi) {
    char *fp = get_full_path(path);
    int fd = open(fp, fi->flags);
    int res = (fd == -1) ? -errno : 0;
    if (fd != -1) close(fd);
    log_op("OPEN", path, res);
    free(fp);
    return res;
}

static int passthrough_read(const char *path, char *buf, size_t size, off_t offset,
                           struct fuse_file_info *fi) {
    char *fp = get_full_path(path);
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
    free(fp);
    return res;
}

static int passthrough_write(const char *path, const char *buf, size_t size, off_t offset,
                            struct fuse_file_info *fi) {
    char *fp = get_full_path(path);
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
    free(fp);
    return res;
}

static int passthrough_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char *fp = get_full_path(path);
    int fd = open(fp, fi->flags, mode);
    int res = (fd == -1) ? -errno : 0;
    if (fd != -1) close(fd);
    log_op("CREATE", path, res);
    free(fp);
    return res;
}

static int passthrough_unlink(const char *path) {
    char *fp = get_full_path(path);
    int res = unlink(fp);
    if (res == -1) res = -errno;
    log_op("UNLINK", path, res);
    free(fp);
    return res;
}

static int passthrough_mkdir(const char *path, mode_t mode) {
    char *fp = get_full_path(path);
    int res = mkdir(fp, mode);
    if (res == -1) res = -errno;
    log_op("MKDIR", path, res);
    free(fp);
    return res;
}

static int passthrough_rmdir(const char *path) {
    char *fp = get_full_path(path);
    int res = rmdir(fp);
    if (res == -1) res = -errno;
    log_op("RMDIR", path, res);
    free(fp);
    return res;
}

static struct fuse_operations ops = {
    .getattr = passthrough_getattr,
    .readdir = passthrough_readdir,
    .open = passthrough_open,
    .read = passthrough_read,
    .write = passthrough_write,
    .create = passthrough_create,
    .unlink = passthrough_unlink,
    .mkdir = passthrough_mkdir,
    .rmdir = passthrough_rmdir,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mount_point> [FUSE options]\n", argv[0]);
        return 1;
    }

    base_path = realpath(argv[1], NULL);
    if (!base_path) {
        fprintf(stderr, "Error: Invalid source directory\n");
        return 1;
    }

    argv[1] = argv[2];
    return fuse_main(argc - 1, argv + 1, &ops, NULL);
}