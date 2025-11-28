#include "myfuse.h"

void get_full_path(char *fpath, const char *path) {
    strcpy(fpath, FS_DATA->rootdir);
    strcat(fpath, path);
}

void log_msg(const char *op, const char *path, int ret) {
    time_t now;
    time(&now);
    char *time_str = ctime(&now);
    time_str[strlen(time_str)-1] = '\0'; // убрать \n
    fprintf(stderr, "[%s] %s: %s (result: %d)\n", time_str, op, path, ret);
}

int do_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    char fpath[MY_MAX_PATH];
    get_full_path(fpath, path);

    int res = lstat(fpath, stbuf);
    
    log_msg("GETATTR", path, res == -1 ? -errno : 0);
    
    if (res == -1)
        return -errno;

    return 0;
}

int do_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags) {
    char fpath[MY_MAX_PATH];
    get_full_path(fpath, path);

    DIR *dp = opendir(fpath);
    if (dp == NULL) {
        log_msg("READDIR", path, -errno);
        return -errno;
    }

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        
        // Добавляем файл в список вывода
        if (filler(buf, de->d_name, &st, 0, 0))
            break;
    }

    closedir(dp);
    log_msg("READDIR", path, 0);
    return 0;
}

int do_mkdir(const char *path, mode_t mode) {
    char fpath[MY_MAX_PATH];
    get_full_path(fpath, path);

    int res = mkdir(fpath, mode);
    log_msg("MKDIR", path, res == -1 ? -errno : 0);
    
    if (res == -1) return -errno;
    return 0;
}

int do_rmdir(const char *path) {
    char fpath[MY_MAX_PATH];
    get_full_path(fpath, path);

    int res = rmdir(fpath);
    log_msg("RMDIR", path, res == -1 ? -errno : 0);

    if (res == -1) return -errno;
    return 0;
}

int do_unlink(const char *path) {
    char fpath[MY_MAX_PATH];
    get_full_path(fpath, path);

    int res = unlink(fpath);
    log_msg("UNLINK", path, res == -1 ? -errno : 0);

    if (res == -1) return -errno;
    return 0;
}

int do_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char fpath[MY_MAX_PATH];
    get_full_path(fpath, path);

    int fd = open(fpath, fi->flags, mode);
    log_msg("CREATE", path, fd == -1 ? -errno : 0);

    if (fd == -1) return -errno;

    fi->fh = fd; // Сохраняем file descriptor для будущих операций
    return 0;
}

int do_open(const char *path, struct fuse_file_info *fi) {
    char fpath[MY_MAX_PATH];
    get_full_path(fpath, path);

    int fd = open(fpath, fi->flags);
    log_msg("OPEN", path, fd == -1 ? -errno : 0);

    if (fd == -1) return -errno;

    fi->fh = fd;
    return 0;
}

int do_read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
    int res = pread(fi->fh, buf, size, offset);
    
    if (res == -1) {
        res = -errno;
    }

    log_msg("READ", path, res);
    return res;
}

int do_write(const char *path, const char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi) {
    int res = pwrite(fi->fh, buf, size, offset);

    if (res == -1)
        res = -errno;

    log_msg("WRITE", path, res);
    return res;
}

struct fuse_operations operations = {
    .getattr = do_getattr,
    .readdir = do_readdir,
    .mkdir   = do_mkdir,
    .rmdir   = do_rmdir,
    .unlink  = do_unlink,
    .create  = do_create,
    .open    = do_open,
    .read    = do_read,
    .write   = do_write,
};

int myfuse_main(int argc, char *argv[]) {
    struct fs_state *fs_data;

    // Проверяем аргументы. Должно быть: ./myfuse [flags] source_dir mount_point
    if ((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-')) {
        fprintf(stderr, "Usage: %s source_dir mount_point [options]\n", argv[0]);
        return 1;
    }

    fs_data = malloc(sizeof(struct fs_state));
    if (fs_data == NULL) {
        perror("malloc");
        return 1;
    }

    char *src_path = realpath(argv[argc-2], NULL);
    if (!src_path) {
        perror("realpath source_dir");
        return 1;
    }
    fs_data->rootdir = src_path;

    char **fuse_argv = malloc(sizeof(char*) * argc);
    int fuse_argc = 0;
    
    for (int i = 0; i < argc; i++) {
        if (i == argc - 2) continue;
        fuse_argv[fuse_argc++] = argv[i];
    }

    fprintf(stderr, "Mounting source: %s\n", fs_data->rootdir);

    int ret = fuse_main(fuse_argc, fuse_argv, &operations, fs_data);
    
    free(src_path);
    free(fs_data);
    free(fuse_argv);
    
    return ret;
}
