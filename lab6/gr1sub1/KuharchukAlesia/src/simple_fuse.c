#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>

static char base_path[PATH_MAX];

void log_operation(const char* operation, const char* path, int result) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    fprintf(stderr, "[%04d-%02d-%02d %02d:%02d:%02d] %s: %s (result: %d)\n",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
            t->tm_hour, t->tm_min, t->tm_sec,
            operation, path, result);
}

void get_full_path(char full_path[PATH_MAX], const char *path) {
    if (snprintf(full_path, PATH_MAX, "%s%s", base_path, path) >= PATH_MAX) {
        full_path[0] = '\0';
    }
}

static int passthrough_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    char full_path[PATH_MAX];
    get_full_path(full_path, path);
    
    int res = lstat(full_path, stbuf);
    log_operation("GETATTR", path, res);
    return (res == -1) ? -errno : 0;
}

static int passthrough_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                              off_t offset, struct fuse_file_info *fi,
                              enum fuse_readdir_flags flags) {
    (void) offset; (void) fi; (void) flags;
    char full_path[PATH_MAX];
    get_full_path(full_path, path);
    
    DIR *dp = opendir(full_path);
    if (!dp) return -errno;
    
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    
    struct dirent *de;
    while ((de = readdir(dp))) {
        struct stat st = {0};
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0, 0)) break;
    }
    
    closedir(dp);
    log_operation("READDIR", path, 0);
    return 0;
}

static int passthrough_open(const char *path, struct fuse_file_info *fi) {
    char full_path[PATH_MAX];
    get_full_path(full_path, path);
    
    int res = open(full_path, fi->flags);
    log_operation("OPEN", path, res);
    
    if (res == -1) return -errno;
    fi->fh = res;
    return 0;
}

static int passthrough_read(const char *path, char *buf, size_t size, off_t offset,
                           struct fuse_file_info *fi) {
    int res = pread(fi->fh, buf, size, offset);
    log_operation("READ", path, res);
    return (res == -1) ? -errno : res;
}

static int passthrough_write(const char *path, const char *buf, size_t size, off_t offset,
                            struct fuse_file_info *fi) {
    int res = pwrite(fi->fh, buf, size, offset);
    log_operation("WRITE", path, res);
    return (res == -1) ? -errno : res;
}

static int passthrough_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char full_path[PATH_MAX];
    get_full_path(full_path, path);
    
    int res = open(full_path, fi->flags, mode);
    log_operation("CREATE", path, res);
    
    if (res == -1) return -errno;
    fi->fh = res;
    return 0;
}

static int passthrough_unlink(const char *path) {
    char full_path[PATH_MAX];
    get_full_path(full_path, path);
    
    int res = unlink(full_path);
    log_operation("UNLINK", path, res);
    return (res == -1) ? -errno : 0;
}

static int passthrough_mkdir(const char *path, mode_t mode) {
    char full_path[PATH_MAX];
    get_full_path(full_path, path);
    
    int res = mkdir(full_path, mode);
    log_operation("MKDIR", path, res);
    return (res == -1) ? -errno : 0;
}

static int passthrough_rmdir(const char *path) {
    char full_path[PATH_MAX];
    get_full_path(full_path, path);
    
    int res = rmdir(full_path);
    log_operation("RMDIR", path, res);
    return (res == -1) ? -errno : 0;
}

static struct fuse_operations operations = {
    .getattr = passthrough_getattr,
    .readdir = passthrough_readdir,
    .open    = passthrough_open,
    .read    = passthrough_read,
    .write   = passthrough_write,
    .create  = passthrough_create,
    .unlink  = passthrough_unlink,
    .mkdir   = passthrough_mkdir,
    .rmdir   = passthrough_rmdir,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mount_point> [FUSE options]\n", argv[0]);
        return 1;
    }

    if (realpath(argv[1], base_path) == NULL) {
        perror("realpath");
        return 1;
    }

    size_t len = strlen(base_path);
    if (len > 0 && base_path[len-1] != '/' && len < PATH_MAX - 1) {
        base_path[len] = '/';
        base_path[len+1] = '\0';
    }

    fprintf(stderr, "Base path: %s\n", base_path);
    fprintf(stderr, "Mount point: %s\n", argv[2]);

    argv[1] = argv[0];
    argc--;
    
    return fuse_main(argc, argv + 1, &operations, NULL);
}
