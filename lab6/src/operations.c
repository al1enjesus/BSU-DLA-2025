#include "operations.h"

// Задание A: Passthrough операции

int passthrough_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    int res;
    char fullpath[1024];
    
    get_full_path(fullpath, path);
    res = lstat(fullpath, stbuf);
    
    log_operation("GETATTR", path, res == 0 ? 0 : -errno);
    
    if (res == -1)
        return -errno;
    
    return 0;
}

int passthrough_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi,
                       enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;
    
    DIR *dp;
    struct dirent *de;
    char fullpath[1024];
    
    get_full_path(fullpath, path);
    
    dp = opendir(fullpath);
    if (dp == NULL) {
        log_operation("READDIR", path, -errno);
        return -errno;
    }
    
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0, FUSE_FILL_DIR_PLUS))
            break;
    }
    
    closedir(dp);
    log_operation("READDIR", path, 0);
    return 0;
}

int passthrough_open(const char *path, struct fuse_file_info *fi) {
    int res;
    char fullpath[1024];
    
    get_full_path(fullpath, path);
    res = open(fullpath, fi->flags);
    
    log_operation("OPEN", path, res == -1 ? -errno : 0);
    
    if (res == -1)
        return -errno;
    
    close(res);
    return 0;
}

int passthrough_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi) {
    (void) fi;
    int fd;
    int res;
    char fullpath[1024];
    
    get_full_path(fullpath, path);
    fd = open(fullpath, O_RDONLY);
    if (fd == -1) {
        log_operation("READ", path, -errno);
        return -errno;
    }
    
    res = pread(fd, buf, size, offset);
    if (res == -1) {
        res = -errno;
    }
    
    close(fd);
    log_operation("READ", path, res);
    return res;
}

int passthrough_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi) {
    (void) fi;
    int fd;
    int res;
    char fullpath[1024];
    
    get_full_path(fullpath, path);
    fd = open(fullpath, O_WRONLY);
    if (fd == -1) {
        log_operation("WRITE", path, -errno);
        return -errno;
    }
    
    res = pwrite(fd, buf, size, offset);
    if (res == -1) {
        res = -errno;
    }
    
    close(fd);
    log_operation("WRITE", path, res);
    return res;
}

int passthrough_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void) fi;
    int res;
    char fullpath[1024];
    
    get_full_path(fullpath, path);
    res = creat(fullpath, mode);
    
    log_operation("CREATE", path, res == -1 ? -errno : 0);
    
    if (res == -1)
        return -errno;
    
    close(res);
    return 0;
}

int passthrough_unlink(const char *path) {
    int res;
    char fullpath[1024];
    
    get_full_path(fullpath, path);
    res = unlink(fullpath);
    
    log_operation("UNLINK", path, res == -1 ? -errno : 0);
    
    if (res == -1)
        return -errno;
    
    return 0;
}

int passthrough_mkdir(const char *path, mode_t mode) {
    int res;
    char fullpath[1024];
    
    get_full_path(fullpath, path);
    res = mkdir(fullpath, mode);
    
    log_operation("MKDIR", path, res == -1 ? -errno : 0);
    
    if (res == -1)
        return -errno;
    
    return 0;
}

int passthrough_rmdir(const char *path) {
    int res;
    char fullpath[1024];
    
    get_full_path(fullpath, path);
    res = rmdir(fullpath);
    
    log_operation("RMDIR", path, res == -1 ? -errno : 0);
    
    if (res == -1)
        return -errno;
    
    return 0;
}

// Задание B: ROT13 операции

int rot13_read(const char *path, char *buf, size_t size, off_t offset,
              struct fuse_file_info *fi) {
    int result = passthrough_read(path, buf, size, offset, fi);
    
    if (result > 0) {
        // Расшифровываем данные при чтении
        rot13_transform(buf, result);
    }
    
    return result;
}

int rot13_write(const char *path, const char *buf, size_t size,
               off_t offset, struct fuse_file_info *fi) {
    char *encrypted_buf = malloc(size);
    if (!encrypted_buf) {
        return -ENOMEM;
    }
    
    // Копируем данные и шифруем их
    memcpy(encrypted_buf, buf, size);
    rot13_transform(encrypted_buf, size);
    
    int result = passthrough_write(path, encrypted_buf, size, offset, fi);
    
    free(encrypted_buf);
    return result;
}

// Задание C: Uppercase операции

int uppercase_read(const char *path, char *buf, size_t size, off_t offset,
                  struct fuse_file_info *fi) {
    int result = passthrough_read(path, buf, size, offset, fi);
    
    if (result > 0) {
        // Преобразуем в верхний регистр при чтении
        uppercase_transform(buf, result);
    }
    
    return result;
}