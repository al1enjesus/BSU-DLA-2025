#include "operations.h"
#include <fuse3/fuse.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

static std::string root_path;

// Установка корневого пути (должен вызываться из main)
void set_root_path(const std::string& path) {
    root_path = path;
}

// Реализации общих функций FUSE
namespace fuse_utils {

int pt_getattr(const char *path, struct stat *stbuf,
               struct fuse_file_info *fi) {
    if (!is_path_safe(path)) {
        log_operation("GETATTR", path, -EACCES);
        return -EACCES;
    }
    
    std::string full_path = translate_path(root_path, path);
    int res;

    if (fi != nullptr && fi->fh > 0) {
        res = fstat(fi->fh, stbuf);
    } else {
        res = lstat(full_path.c_str(), stbuf);
    }

    int ret = res == -1 ? -errno : 0;
    log_operation("GETATTR", path, ret);
    return ret;
}

int pt_open(const char *path, struct fuse_file_info *fi) {
    if (!is_path_safe(path)) {
        log_operation("OPEN", path, -EACCES);
        return -EACCES;
    }
    
    std::string full_path = translate_path(root_path, path);
    int fd = open(full_path.c_str(), fi->flags);

    int ret = 0;
    if (fd == -1) {
        ret = -errno;
    } else {
        fi->fh = fd;
    }

    log_operation("OPEN", path, ret);
    return ret;
}

int pt_read(const char *path, char *buf, size_t size,
            off_t offset, struct fuse_file_info *fi) {
    (void)path; // Не используется, но нужен для сигнатуры
    
    int fd = fi->fh;
    int res = pread(fd, buf, size, offset);

    int ret = res == -1 ? -errno : res;
    log_operation("READ", path, ret);
    return ret;
}

int pt_write(const char *path, const char *buf, size_t size,
             off_t offset, struct fuse_file_info *fi) {
    (void)path; // Не используется, но нужен для сигнатуры
    
    int fd = fi->fh;
    int res = pwrite(fd, buf, size, offset);

    int ret = res == -1 ? -errno : res;
    log_operation("WRITE", path, ret);
    return ret;
}

int pt_release(const char *path, struct fuse_file_info *fi) {
    (void)path; // Не используется, но нужен для сигнатуры
    
    close(fi->fh);
    log_operation("RELEASE", path, 0);
    return 0;
}

int pt_mkdir(const char *path, mode_t mode) {
    if (!is_path_safe(path)) {
        log_operation("MKDIR", path, -EACCES);
        return -EACCES;
    }
    
    std::string full_path = translate_path(root_path, path);
    int res = mkdir(full_path.c_str(), mode);

    int ret = res == -1 ? -errno : 0;
    log_operation("MKDIR", path, ret);
    return ret;
}

int pt_rmdir(const char *path) {
    if (!is_path_safe(path)) {
        log_operation("RMDIR", path, -EACCES);
        return -EACCES;
    }
    
    std::string full_path = translate_path(root_path, path);
    int res = rmdir(full_path.c_str());

    int ret = res == -1 ? -errno : 0;
    log_operation("RMDIR", path, ret);
    return ret;
}

int pt_unlink(const char *path) {
    if (!is_path_safe(path)) {
        log_operation("UNLINK", path, -EACCES);
        return -EACCES;
    }
    
    std::string full_path = translate_path(root_path, path);
    int res = unlink(full_path.c_str());

    int ret = res == -1 ? -errno : 0;
    log_operation("UNLINK", path, ret);
    return ret;
}

int pt_rename(const char *from, const char *to, unsigned int flags) {
    (void)flags; // Не используется
    
    if (!is_path_safe(from) || !is_path_safe(to)) {
        log_operation("RENAME", from, -EACCES);
        return -EACCES;
    }
    
    std::string from_path = translate_path(root_path, from);
    std::string to_path = translate_path(root_path, to);
    int res = rename(from_path.c_str(), to_path.c_str());

    int ret = res == -1 ? -errno : 0;
    log_operation("RENAME", from, ret);
    return ret;
}

int pt_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
               off_t offset, struct fuse_file_info *fi,
               enum fuse_readdir_flags flags) {
    (void)offset; // Не используется
    (void)fi;     // Не используется
    (void)flags;  // Не используется
    
    if (!is_path_safe(path)) {
        log_operation("READDIR", path, -EACCES);
        return -EACCES;
    }
    
    std::string full_path = translate_path(root_path, path);

    DIR *dp = opendir(full_path.c_str());
    if (dp == nullptr) {
        int ret = -errno;
        log_operation("READDIR", path, ret);
        return ret;
    }

    struct dirent *de;
    while ((de = readdir(dp)) != nullptr) {
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

int pt_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    if (!is_path_safe(path)) {
        log_operation("CREATE", path, -EACCES);
        return -EACCES;
    }
    
    std::string full_path = translate_path(root_path, path);
    int fd = open(full_path.c_str(), fi->flags, mode);

    int ret = 0;
    if (fd == -1) {
        ret = -errno;
    } else {
        fi->fh = fd;
    }

    log_operation("CREATE", path, ret);
    return ret;
}

int pt_truncate(const char *path, off_t size,
                struct fuse_file_info *fi) {
    if (!is_path_safe(path)) {
        log_operation("TRUNCATE", path, -EACCES);
        return -EACCES;
    }
    
    int res;
    if (fi != nullptr && fi->fh > 0) {
        res = ftruncate(fi->fh, size);
    } else {
        std::string full_path = translate_path(root_path, path);
        res = truncate(full_path.c_str(), size);
    }

    int ret = res == -1 ? -errno : 0;
    log_operation("TRUNCATE", path, ret);
    return ret;
}

} // namespace fuse_utils