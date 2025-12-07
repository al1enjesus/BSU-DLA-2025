#include "../include/fuse_fs.h"

// ... (аналогично предыдущему варианту, но с учётом .stats)

int passthrough_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    if (is_stats_file(path)) {
        return stats_getattr(path, stbuf);
    }

    char full[PATH_MAX];
    snprintf(full, sizeof(full), "%s%s", config.source_path, path);
    int res = lstat(full, stbuf);
    int err = (res == -1) ? -errno : 0;
    log_op("GETATTR", path, err);
    return err;
}

int passthrough_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
                        struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    if (strcmp(path, "/") == 0) {
        // Добавляем .stats в корень
        filler(buf, ".stats", NULL, 0, 0);
    }

    char full[PATH_MAX];
    snprintf(full, sizeof(full), "%s%s", config.source_path, path);
    DIR *dp = opendir(full);
    if (!dp) return -errno;

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
    return 0;
}

// ... остальные функции (open, read, write...) — как в прошлом варианте,
// но с учётом stats-счётчиков и проверкой is_stats_file в read/write

int passthrough_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    if (is_stats_file(path)) {
        return stats_read(path, buf, size, offset);
    }

    int res = pread(fi->fh, buf, size, offset);
    if (res > 0) {
        pthread_mutex_lock(&stats_mutex);
        stats_reads++;
        stats_bytes_read += res;
        pthread_mutex_unlock(&stats_mutex);
    }
    log_op("READ", path, res);
    return (res == -1) ? -errno : res;
}

// Аналогично для write, create и т.д.

struct fuse_operations passthrough_ops = {
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