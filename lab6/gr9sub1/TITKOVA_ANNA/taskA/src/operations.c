#include "operations.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/xattr.h>

/* getattr - получение атрибутов файла */
int passthrough_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void)fi;
    
    if (!check_path_safety(path)) {
        return -EACCES;
    }
    
    char *fullpath = get_full_path(path);
    if (!fullpath) {
        return -ENOMEM;
    }
    
    int res = lstat(fullpath, stbuf);
    log_operation("GETATTR", path, res);
    
    free(fullpath);
    
    if (res == -1) {
        return -errno;
    }
    
    return 0;
}

/* readdir - чтение содержимого директории */
int passthrough_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi,
                        enum fuse_readdir_flags flags) {
    (void)offset;
    (void)fi;
    (void)flags;
    
    if (!check_path_safety(path)) {
        return -EACCES;
    }
    
    char *fullpath = get_full_path(path);
    if (!fullpath) {
        return -ENOMEM;
    }
    
    DIR *dp = opendir(fullpath);
    if (dp == NULL) {
        log_operation("READDIR", path, -errno);
        free(fullpath);
        return -errno;
    }
    
    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        
        if (filler(buf, de->d_name, &st, 0, 0))
            break;
    }
    
    closedir(dp);
    log_operation("READDIR", path, 0);
    free(fullpath);
    return 0;
}

/* open - открытие файла */
int passthrough_open(const char *path, struct fuse_file_info *fi) {
    if (!check_path_safety(path)) {
        return -EACCES;
    }
    
    char *fullpath = get_full_path(path);
    if (!fullpath) {
        return -ENOMEM;
    }
    
    int fd = open(fullpath, fi->flags);
    free(fullpath);
    
    if (fd == -1) {
        log_operation("OPEN", path, -errno);
        return -errno;
    }
    
    fi->fh = fd;
    log_operation("OPEN", path, 0);
    return 0;
}

/* read - чтение из файла */
int passthrough_read(const char *path, char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi) {
    int res = pread(fi->fh, buf, size, offset);
    
    if (res == -1) {
        res = -errno;
    }
    
    log_operation_bytes("READ", path, size, offset, res);
    return res;
}

/* write - запись в файл */
int passthrough_write(const char *path, const char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
    int res = pwrite(fi->fh, buf, size, offset);
    
    if (res == -1) {
        res = -errno;
    }
    
    log_operation_bytes("WRITE", path, size, offset, res);
    return res;
}

/* create - создание файла */
int passthrough_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    if (!check_path_safety(path)) {
        return -EACCES;
    }
    
    char *fullpath = get_full_path(path);
    if (!fullpath) {
        return -ENOMEM;
    }
    
    int fd = creat(fullpath, mode);
    free(fullpath);
    
    if (fd == -1) {
        log_operation("CREATE", path, -errno);
        return -errno;
    }
    
    fi->fh = fd;
    log_operation("CREATE", path, 0);
    return 0;
}

/* unlink - удаление файла */
int passthrough_unlink(const char *path) {
    if (!check_path_safety(path)) {
        return -EACCES;
    }
    
    char *fullpath = get_full_path(path);
    if (!fullpath) {
        return -ENOMEM;
    }
    
    int res = unlink(fullpath);
    log_operation("UNLINK", path, res);
    free(fullpath);
    
    if (res == -1) {
        return -errno;
    }
    
    return 0;
}

/* mkdir - создание директории */
int passthrough_mkdir(const char *path, mode_t mode) {
    if (!check_path_safety(path)) {
        return -EACCES;
    }
    
    char *fullpath = get_full_path(path);
    if (!fullpath) {
        return -ENOMEM;
    }
    
    int res = mkdir(fullpath, mode);
    log_operation("MKDIR", path, res);
    free(fullpath);
    
    if (res == -1) {
        return -errno;
    }
    
    return 0;
}

/* rmdir - удаление директории */
int passthrough_rmdir(const char *path) {
    if (!check_path_safety(path)) {
        return -EACCES;
    }
    
    char *fullpath = get_full_path(path);
    if (!fullpath) {
        return -ENOMEM;
    }
    
    int res = rmdir(fullpath);
    log_operation("RMDIR", path, res);
    free(fullpath);
    
    if (res == -1) {
        return -errno;
    }
    
    return 0;
}

/* rename - переименование/перемещение */
int passthrough_rename(const char *oldpath, const char *newpath, unsigned int flags) {
    (void)flags; /* Пока игнорируем флаги RENAME_EXCHANGE, RENAME_NOREPLACE */
    
    if (!check_path_safety(oldpath) || !check_path_safety(newpath)) {
        return -EACCES;
    }
    
    char *full_oldpath = get_full_path(oldpath);
    char *full_newpath = get_full_path(newpath);
    
    if (!full_oldpath || !full_newpath) {
        free(full_oldpath);
        free(full_newpath);
        return -ENOMEM;
    }
    
    int res = rename(full_oldpath, full_newpath);
    log_operation("RENAME", oldpath, res);
    
    free(full_oldpath);
    free(full_newpath);
    
    if (res == -1) {
        return -errno;
    }
    
    return 0;
}

/* chmod - изменение прав доступа */
int passthrough_chmod(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void)fi;
    
    if (!check_path_safety(path)) {
        return -EACCES;
    }
    
    char *fullpath = get_full_path(path);
    if (!fullpath) {
        return -ENOMEM;
    }
    
    int res = chmod(fullpath, mode);
    log_operation("CHMOD", path, res);
    free(fullpath);
    
    if (res == -1) {
        return -errno;
    }
    
    return 0;
}

/* chown - изменение владельца */
int passthrough_chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi) {
    (void)fi;
    
    if (!check_path_safety(path)) {
        return -EACCES;
    }
    
    char *fullpath = get_full_path(path);
    if (!fullpath) {
        return -ENOMEM;
    }
    
    int res = lchown(fullpath, uid, gid);
    log_operation("CHOWN", path, res);
    free(fullpath);
    
    if (res == -1) {
        return -errno;
    }
    
    return 0;
}

/* truncate - изменение размера файла */
int passthrough_truncate(const char *path, off_t size, struct fuse_file_info *fi) {
    if (!check_path_safety(path)) {
        return -EACCES;
    }
    
    char *fullpath = get_full_path(path);
    if (!fullpath) {
        return -ENOMEM;
    }
    
    int res;
    if (fi != NULL && fi->fh != 0) {
        res = ftruncate(fi->fh, size);
    } else {
        res = truncate(fullpath, size);
    }
    
    log_operation("TRUNCATE", path, res);
    free(fullpath);
    
    if (res == -1) {
        return -errno;
    }
    
    return 0;
}

/* utimens - изменение временных меток */
int passthrough_utimens(const char *path, const struct timespec tv[2],
                        struct fuse_file_info *fi) {
    (void)fi;
    
    if (!check_path_safety(path)) {
        return -EACCES;
    }
    
    char *fullpath = get_full_path(path);
    if (!fullpath) {
        return -ENOMEM;
    }
    
    int res = utimensat(AT_FDCWD, fullpath, tv, AT_SYMLINK_NOFOLLOW);
    log_operation("UTIMENS", path, res);
    free(fullpath);
    
    if (res == -1) {
        return -errno;
    }
    
    return 0;
}

/* statfs - получение статистики ФС */
int passthrough_statfs(const char *path, struct statvfs *stbuf) {
    if (!check_path_safety(path)) {
        return -EACCES;
    }
    
    char *fullpath = get_full_path(path);
    if (!fullpath) {
        return -ENOMEM;
    }
    
    int res = statvfs(fullpath, stbuf);
    log_operation("STATFS", path, res);
    free(fullpath);
    
    if (res == -1) {
        return -errno;
    }
    
    return 0;
}

/* release - закрытие файла */
int passthrough_release(const char *path, struct fuse_file_info *fi) {
    (void)path;
    
    if (fi->fh != 0) {
        close(fi->fh);
        fi->fh = 0;
    }
    
    log_operation("RELEASE", path, 0);
    return 0;
}

/* flush - сброс буферов */
int passthrough_flush(const char *path, struct fuse_file_info *fi) {
    (void)path;
    (void)fi;
    log_operation("FLUSH", path, 0);
    return 0;
}

/* fsync - синхронизация файла */
int passthrough_fsync(const char *path, int isdatasync, struct fuse_file_info *fi) {
    (void)path;
    
    int res;
    if (isdatasync) {
        res = fdatasync(fi->fh);
    } else {
        res = fsync(fi->fh);
    }
    
    log_operation("FSYNC", path, res);
    
    if (res == -1) {
        return -errno;
    }
    
    return 0;
}
