#define FUSE_USE_VERSION 35
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
#include "utils.h"

static std::string root_path;

// getattr - получение атрибутов файла
static int pt_getattr(const char *path, struct stat *stbuf,
                      struct fuse_file_info *fi) {
    if (!fuse_utils::check_null(path, "getattr: path") || 
        !fuse_utils::check_null(stbuf, "getattr: stbuf")) {
        return -EINVAL;
    }
    
    if (!fuse_utils::is_path_safe(path)) {
        fuse_utils::log_operation("GETATTR", path, -EACCES);
        return -EACCES;
    }
    
    std::string full_path = fuse_utils::translate_path(root_path, path);
    int res;

    if (fi && fi->fh > 0) {
        res = fstat(fi->fh, stbuf);
    } else {
        res = lstat(full_path.c_str(), stbuf);
    }

    int ret = res == -1 ? -errno : 0;
    fuse_utils::log_operation("GETATTR", path, ret);
    return ret;
}

// open - открытие файла
static int pt_open(const char *path, struct fuse_file_info *fi) {
    if (!fuse_utils::check_null(path, "open: path") || 
        !fuse_utils::check_null(fi, "open: fi")) {
        return -EINVAL;
    }
    
    if (!fuse_utils::is_path_safe(path)) {
        fuse_utils::log_operation("OPEN", path, -EACCES);
        return -EACCES;
    }
    
    std::string full_path = fuse_utils::translate_path(root_path, path);
    int fd = open(full_path.c_str(), fi->flags);

    int ret = 0;
    if (fd == -1) {
        ret = -errno;
    } else {
        fi->fh = fd;
    }

    fuse_utils::log_operation("OPEN", path, ret);
    return ret;
}

// read - чтение из файла
static int pt_read(const char *path, char *buf, size_t size,
                   off_t offset, struct fuse_file_info *fi) {
    if (!fuse_utils::check_null(buf, "read: buf") || 
        !fuse_utils::check_null(fi, "read: fi")) {
        return -EINVAL;
    }
    
    int fd = fi->fh;
    int res = pread(fd, buf, size, offset);

    int ret = res == -1 ? -errno : res;
    fuse_utils::log_operation("READ", path, ret);
    return ret;
}

// write - запись в файл
static int pt_write(const char *path, const char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi) {
    if (!fuse_utils::check_null(buf, "write: buf") || 
        !fuse_utils::check_null(fi, "write: fi")) {
        return -EINVAL;
    }
    
    int fd = fi->fh;
    int res = pwrite(fd, buf, size, offset);

    int ret = res == -1 ? -errno : res;
    fuse_utils::log_operation("WRITE", path, ret);
    return ret;
}

// release - закрытие файла
static int pt_release(const char *path, struct fuse_file_info *fi) {
    if (!fuse_utils::check_null(fi, "release: fi")) {
        return -EINVAL;
    }
    
    close(fi->fh);
    fuse_utils::log_operation("RELEASE", path, 0);
    return 0;
}

// mkdir - создание директории
static int pt_mkdir(const char *path, mode_t mode) {
    if (!fuse_utils::check_null(path, "mkdir: path")) {
        return -EINVAL;
    }
    
    if (!fuse_utils::is_path_safe(path)) {
        fuse_utils::log_operation("MKDIR", path, -EACCES);
        return -EACCES;
    }
    
    std::string full_path = fuse_utils::translate_path(root_path, path);
    int res = mkdir(full_path.c_str(), mode);

    int ret = res == -1 ? -errno : 0;
    fuse_utils::log_operation("MKDIR", path, ret);
    return ret;
}

// rmdir - удаление директории
static int pt_rmdir(const char *path) {
    if (!fuse_utils::check_null(path, "rmdir: path")) {
        return -EINVAL;
    }
    
    if (!fuse_utils::is_path_safe(path)) {
        fuse_utils::log_operation("RMDIR", path, -EACCES);
        return -EACCES;
    }
    
    std::string full_path = fuse_utils::translate_path(root_path, path);
    int res = rmdir(full_path.c_str());

    int ret = res == -1 ? -errno : 0;
    fuse_utils::log_operation("RMDIR", path, ret);
    return ret;
}

// unlink - удаление файла
static int pt_unlink(const char *path) {
    if (!fuse_utils::check_null(path, "unlink: path")) {
        return -EINVAL;
    }
    
    if (!fuse_utils::is_path_safe(path)) {
        fuse_utils::log_operation("UNLINK", path, -EACCES);
        return -EACCES;
    }
    
    std::string full_path = fuse_utils::translate_path(root_path, path);
    int res = unlink(full_path.c_str());

    int ret = res == -1 ? -errno : 0;
    fuse_utils::log_operation("UNLINK", path, ret);
    return ret;
}

// rename - переименование/перемещение файла
static int pt_rename(const char *from, const char *to, unsigned int flags) {
    (void)flags; // FUSE может использовать этот параметр
    
    if (!fuse_utils::check_null(from, "rename: from") || 
        !fuse_utils::check_null(to, "rename: to")) {
        return -EINVAL;
    }
    
    if (!fuse_utils::is_path_safe(from) || !fuse_utils::is_path_safe(to)) {
        fuse_utils::log_operation("RENAME", from, -EACCES);
        return -EACCES;
    }
    
    std::string from_path = fuse_utils::translate_path(root_path, from);
    std::string to_path = fuse_utils::translate_path(root_path, to);
    int res = rename(from_path.c_str(), to_path.c_str());

    int ret = res == -1 ? -errno : 0;
    fuse_utils::log_operation("RENAME", from, ret);
    return ret;
}

// readdir - чтение содержимого директории
static int pt_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags) {
    (void)offset; // Параметр может использоваться для больших директорий
    (void)fi;     // Может использоваться в некоторых реализациях
    (void)flags;  // Флаги чтения директории
    
    if (!fuse_utils::check_null(path, "readdir: path") || 
        !fuse_utils::check_null(buf, "readdir: buf") ||
        !fuse_utils::check_filler(filler, "readdir: filler")) {
        return -EINVAL;
    }
    
    if (!fuse_utils::is_path_safe(path)) {
        fuse_utils::log_operation("READDIR", path, -EACCES);
        return -EACCES;
    }
    
    std::string full_path = fuse_utils::translate_path(root_path, path);

    DIR *dp = opendir(full_path.c_str());
    if (!dp) {
        int ret = -errno;
        fuse_utils::log_operation("READDIR", path, ret);
        return ret;
    }

    struct dirent *de;
    while ((de = readdir(dp)) != nullptr) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;

        if (filler(buf, de->d_name, &st, 0, FUSE_FILL_DIR_PLUS)) {
            break;
        }
    }

    closedir(dp);
    fuse_utils::log_operation("READDIR", path, 0);
    return 0;
}

// create - создание нового файла
static int pt_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    if (!fuse_utils::check_null(path, "create: path") || 
        !fuse_utils::check_null(fi, "create: fi")) {
        return -EINVAL;
    }
    
    if (!fuse_utils::is_path_safe(path)) {
        fuse_utils::log_operation("CREATE", path, -EACCES);
        return -EACCES;
    }
    
    std::string full_path = fuse_utils::translate_path(root_path, path);
    int fd = open(full_path.c_str(), fi->flags, mode);

    int ret = 0;
    if (fd == -1) {
        ret = -errno;
    } else {
        fi->fh = fd;
    }

    fuse_utils::log_operation("CREATE", path, ret);
    return ret;
}

// truncate - изменение размера файла
static int pt_truncate(const char *path, off_t size,
                       struct fuse_file_info *fi) {
    if (!fuse_utils::check_null(path, "truncate: path")) {
        return -EINVAL;
    }
    
    if (!fuse_utils::is_path_safe(path)) {
        fuse_utils::log_operation("TRUNCATE", path, -EACCES);
        return -EACCES;
    }
    
    int res;
    if (fi && fi->fh > 0) {
        res = ftruncate(fi->fh, size);
    } else {
        std::string full_path = fuse_utils::translate_path(root_path, path);
        res = truncate(full_path.c_str(), size);
    }

    int ret = res == -1 ? -errno : 0;
    fuse_utils::log_operation("TRUNCATE", path, ret);
    return ret;
}

// Структура операций FUSE
static struct fuse_operations pt_oper = {
    .getattr    = pt_getattr,
    .mkdir      = pt_mkdir,
    .unlink     = pt_unlink,
    .rmdir      = pt_rmdir,
    .rename     = pt_rename,
    .truncate   = pt_truncate,
    .open       = pt_open,
    .read       = pt_read,
    .write      = pt_write,
    .release    = pt_release,
    .readdir    = pt_readdir,
    .create     = pt_create,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mount_point> [fuse_options]\n", argv[0]);
        fprintf(stderr, "Example: %s /tmp/source /mnt/fuse -f\n", argv[0]);
        fprintf(stderr, "\nPassthrough Filesystem:\n");
        fprintf(stderr, "  - Mirrors existing directory\n");
        fprintf(stderr, "  - All operations are logged\n");
        fprintf(stderr, "  - Path traversal protection enabled\n");
        return 1;
    }

    // Сохраняем путь к source директории
    root_path = argv[1];

    // Проверяем существование директории
    struct stat st;
    if (stat(root_path.c_str(), &st) == -1) {
        fprintf(stderr, "Error: Source directory '%s' does not exist\n", root_path.c_str());
        return 1;
    }

    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: '%s' is not a directory\n", root_path.c_str());
        return 1;
    }

    fprintf(stderr, "Mounting passthrough FS: %s -> %s\n", 
            root_path.c_str(), argv[2]);

    // Передаем FUSE только mountpoint и опции (убираем source_dir)
    int fuse_argc = argc - 1;
    char **fuse_argv = (char**)malloc(sizeof(char*) * fuse_argc);
    if (!fuse_argv) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return 1;
    }
    
    fuse_argv[0] = argv[0];
    for (int i = 2; i < argc; i++) {
        fuse_argv[i-1] = argv[i];
    }

    int ret = fuse_main(fuse_argc, fuse_argv, &pt_oper, nullptr);

    free(fuse_argv);
    return ret;
}