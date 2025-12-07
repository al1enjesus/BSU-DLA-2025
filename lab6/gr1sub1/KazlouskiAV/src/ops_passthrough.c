#include "../include/fuse_fs.h"

// =============== GETATTR ===============
int passthrough_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    if (is_stats_file(path)) {
        return stats_getattr(path, stbuf);
    }

    char full[PATH_MAX];
    if (snprintf(full, sizeof(full), "%s%s", config.source_path, path) >= (int)sizeof(full)) {
        log_op("GETATTR", path, -ENAMETOOLONG);
        return -ENAMETOOLONG;
    }

    int res = lstat(full, stbuf);
    int err = (res == -1) ? -errno : 0;
    log_op("GETATTR", path, err);
    return err;
}

// =============== READDIR ===============
int passthrough_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
                        struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    // Добавляем .stats только в корень
    if (strcmp(path, "/") == 0) {
        filler(buf, ".stats", NULL, 0, 0);
    }

    char full[PATH_MAX];
    if (snprintf(full, sizeof(full), "%s%s", config.source_path, path) >= (int)sizeof(full)) {
        log_op("READDIR", path, -ENAMETOOLONG);
        return -ENAMETOOLONG;
    }

    DIR *dp = opendir(full);
    if (!dp) {
        int err = -errno;
        log_op("READDIR", path, err);
        return err;
    }

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0, 0) != 0) {
            break;
        }
    }
    closedir(dp);
    log_op("READDIR", path, 0);
    return 0;
}

// =============== OPEN ===============
int passthrough_open(const char *path, struct fuse_file_info *fi) {
    if (is_stats_file(path)) {
        // .stats — виртуальный файл, не требует fd
        return 0;
    }

    char full[PATH_MAX];
    if (snprintf(full, sizeof(full), "%s%s", config.source_path, path) >= (int)sizeof(full)) {
        log_op("OPEN", path, -ENAMETOOLONG);
        return -ENAMETOOLONG;
    }

    int fd = open(full, fi->flags);
    if (fd == -1) {
        int err = -errno;
        log_op("OPEN", path, err);
        return err;
    }
    fi->fh = fd;

    pthread_mutex_lock(&stats_mutex);
    stats_opens++;
    pthread_mutex_unlock(&stats_mutex);

    log_op("OPEN", path, 0);
    return 0;
}

// =============== READ ===============
int passthrough_read(const char *path, char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi) {
    if (is_stats_file(path)) {
        return stats_read(path, buf, size, offset);
    }

    int res = pread(fi->fh, buf, size, offset);
    if (res == -1) {
        int err = -errno;
        log_op("READ", path, err);
        return err;
    }

    if (res > 0) {
        pthread_mutex_lock(&stats_mutex);
        stats_reads++;
        stats_bytes_read += res;
        pthread_mutex_unlock(&stats_mutex);
    }

    log_op("READ", path, res);
    return res;
}

// =============== WRITE ===============
int passthrough_write(const char *path, const char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
    if (is_stats_file(path)) {
        // .stats — read-only
        log_op("WRITE", path, -EACCES);
        return -EACCES;
    }

    int res = pwrite(fi->fh, buf, size, offset);
    if (res == -1) {
        int err = -errno;
        log_op("WRITE", path, err);
        return err;
    }

    if (res > 0) {
        pthread_mutex_lock(&stats_mutex);
        stats_writes++;
        stats_bytes_written += res;
        pthread_mutex_unlock(&stats_mutex);
    }

    log_op("WRITE", path, res);
    return res;
}

// =============== CREATE ===============
int passthrough_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char full[PATH_MAX];
    if (snprintf(full, sizeof(full), "%s%s", config.source_path, path) >= (int)sizeof(full)) {
        log_op("CREATE", path, -ENAMETOOLONG);
        return -ENAMETOOLONG;
    }

    int fd = open(full, fi->flags, mode);
    if (fd == -1) {
        int err = -errno;
        log_op("CREATE", path, err);
        return err;
    }
    fi->fh = fd;
    log_op("CREATE", path, 0);
    return 0;
}

// =============== UNLINK ===============
int passthrough_unlink(const char *path) {
    char full[PATH_MAX];
    if (snprintf(full, sizeof(full), "%s%s", config.source_path, path) >= (int)sizeof(full)) {
        log_op("UNLINK", path, -ENAMETOOLONG);
        return -ENAMETOOLONG;
    }

    int res = unlink(full);
    int err = (res == -1) ? -errno : 0;
    log_op("UNLINK", path, err);
    return err;
}

// =============== MKDIR ===============
int passthrough_mkdir(const char *path, mode_t mode) {
    char full[PATH_MAX];
    if (snprintf(full, sizeof(full), "%s%s", config.source_path, path) >= (int)sizeof(full)) {
        log_op("MKDIR", path, -ENAMETOOLONG);
        return -ENAMETOOLONG;
    }

    int res = mkdir(full, mode);
    int err = (res == -1) ? -errno : 0;
    log_op("MKDIR", path, err);
    return err;
}

// =============== RMDIR ===============
int passthrough_rmdir(const char *path) {
    char full[PATH_MAX];
    if (snprintf(full, sizeof(full), "%s%s", config.source_path, path) >= (int)sizeof(full)) {
        log_op("RMDIR", path, -ENAMETOOLONG);
        return -ENAMETOOLONG;
    }

    int res = rmdir(full);
    int err = (res == -1) ? -errno : 0;
    log_op("RMDIR", path, err);
    return err;
}

// =============== OPERATIONS STRUCTURE ===============
struct fuse_operations passthrough_ops = {
    .getattr  = passthrough_getattr,
    .readdir  = passthrough_readdir,
    .open     = passthrough_open,
    .read     = passthrough_read,
    .write    = passthrough_write,
    .create   = passthrough_create,
    .unlink   = passthrough_unlink,
    .mkdir    = passthrough_mkdir,
    .rmdir    = passthrough_rmdir,
};