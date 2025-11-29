#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <dirent.h>

static char *source_dir = NULL;

// Логирование
static void log_msg(const char *op, const char *path) {
    time_t t = time(NULL);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", localtime(&t));
    fprintf(stderr, "[%s] %s: %s\n", time_buf, op, path);
}

// Получение полного пути
static void get_full_path(char *fpath, const char *path) {
    strcpy(fpath, source_dir);
    if (fpath[strlen(fpath) - 1] == '/') fpath[strlen(fpath) - 1] = '\0';
    strcat(fpath, path);
}

static int do_getattr(const char *path, struct stat *st, struct fuse_file_info *fi) {
    (void) fi; // Этот параметр не используется, глушим предупреждение компилятора
    
    char fpath[1024];
    get_full_path(fpath, path);
    
    if (lstat(fpath, st) == -1)
        return -errno;
        
    log_msg("GETATTR", path);
    return 0;
}

static int do_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    char fpath[1024];
    get_full_path(fpath, path);
    DIR *dp = opendir(fpath);
    if (!dp) return -errno;

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0, 0)) break;
    }
    closedir(dp);
    log_msg("READDIR", path);
    return 0;
}

static int do_open(const char *path, struct fuse_file_info *fi) {
    char fpath[1024];
    get_full_path(fpath, path);
    int fd = open(fpath, fi->flags);
    if (fd == -1) return -errno;
    fi->fh = fd;
    log_msg("OPEN", path);
    return 0;
}

static int do_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    int res = pread(fi->fh, buf, size, offset);
    if (res == -1) res = -errno;
    log_msg("READ", path);
    return res;
}

static int do_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    int res = pwrite(fi->fh, buf, size, offset);
    if (res == -1) res = -errno;
    log_msg("WRITE", path);
    return res;
}

static int do_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char fpath[1024]; get_full_path(fpath, path);
    int res = open(fpath, fi->flags, mode);
    if (res == -1) return -errno;
    fi->fh = res;
    log_msg("CREATE", path);
    return 0;
}

static int do_unlink(const char *path) {
    char fpath[1024]; get_full_path(fpath, path);
    int res = unlink(fpath);
    log_msg("UNLINK", path);
    return (res == -1) ? -errno : 0;
}

static int do_mkdir(const char *path, mode_t mode) {
    char fpath[1024]; get_full_path(fpath, path);
    int res = mkdir(fpath, mode);
    log_msg("MKDIR", path);
    return (res == -1) ? -errno : 0;
}

static int do_rmdir(const char *path) {
    char fpath[1024]; get_full_path(fpath, path);
    int res = rmdir(fpath);
    log_msg("RMDIR", path);
    return (res == -1) ? -errno : 0;
}

static struct fuse_operations operations = {
    .getattr = do_getattr,
    .readdir = do_readdir,
    .open    = do_open,
    .read    = do_read,
    .write   = do_write,
    .create  = do_create,
    .unlink  = do_unlink,
    .mkdir   = do_mkdir,
    .rmdir   = do_rmdir,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mount_point>\n", argv[0]);
        return 1;
    }
    source_dir = realpath(argv[1], NULL);
    char *fuse_argv[] = { argv[0], argv[2], "-f", NULL }; // Force foreground
    return fuse_main(3, fuse_argv, &operations, NULL);
}
