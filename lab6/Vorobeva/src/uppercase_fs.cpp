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
#include <cctype>
#include "utils.h"

static std::string root_path;

// Преобразование буфера в верхний регистр с проверками
inline void to_uppercase(char *buf, size_t size) {
    if (!fuse_utils::check_null(buf, "to_uppercase: buf") || size == 0) {
        return;
    }
    
    for (size_t i = 0; i < size; i++) {
        buf[i] = toupper((unsigned char)buf[i]);
    }
}

// getattr - получение атрибутов файла
static int up_getattr(const char *path, struct stat *stbuf,
                      struct fuse_file_info *fi) {
    if (!fuse_utils::check_null(path, "up_getattr: path") || 
        !fuse_utils::check_null(stbuf, "up_getattr: stbuf")) {
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
static int up_open(const char *path, struct fuse_file_info *fi) {
    if (!fuse_utils::check_null(path, "up_open: path") || 
        !fuse_utils::check_null(fi, "up_open: fi")) {
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

// read - чтение из файла с преобразованием в uppercase
static int up_read(const char *path, char *buf, size_t size,
                   off_t offset, struct fuse_file_info *fi) {
    if (!fuse_utils::check_null(buf, "up_read: buf") || 
        !fuse_utils::check_null(fi, "up_read: fi")) {
        return -EINVAL;
    }
    
    int fd = fi->fh;
    int res = pread(fd, buf, size, offset);

    if (res > 0) {
        // Преобразуем данные в верхний регистр при чтении
        to_uppercase(buf, res);
    }

    int ret = res == -1 ? -errno : res;
    fuse_utils::log_operation("READ", path, ret);
    return ret;
}

// write - запись в файл (без преобразования)
static int up_write(const char *path, const char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi) {
    if (!fuse_utils::check_null(buf, "up_write: buf") || 
        !fuse_utils::check_null(fi, "up_write: fi")) {
        return -EINVAL;
    }
    
    int fd = fi->fh;
    
    // Записываем данные как есть (без преобразования)
    int res = pwrite(fd, buf, size, offset);

    int ret = res == -1 ? -errno : res;
    fuse_utils::log_operation("WRITE", path, ret);
    return ret;
}

// release - закрытие файла
static int up_release(const char *path, struct fuse_file_info *fi) {
    if (!fuse_utils::check_null(fi, "up_release: fi")) {
        return -EINVAL;
    }
    
    close(fi->fh);
    fuse_utils::log_operation("RELEASE", path, 0);
    return 0;
}

// mkdir - создание директории
static int up_mkdir(const char *path, mode_t mode) {
    if (!fuse_utils::check_null(path, "up_mkdir: path")) {
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
static int up_rmdir(const char *path) {
    if (!fuse_utils::check_null(path, "up_rmdir: path")) {
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
static int up_unlink(const char *path) {
    if (!fuse_utils::check_null(path, "up_unlink: path")) {
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
static int up_rename(const char *from, const char *to, unsigned int flags) {
    if (!fuse_utils::check_null(from, "up_rename: from") || 
        !fuse_utils::check_null(to, "up_rename: to")) {
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
static int up_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags) {
    (void)offset;
    (void)fi;
    (void)flags;
    
    if (!fuse_utils::check_null(path, "up_readdir: path") || 
        !fuse_utils::check_null(buf, "up_readdir: buf")) {
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
static int up_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    if (!fuse_utils::check_null(path, "up_create: path") || 
        !fuse_utils::check_null(fi, "up_create: fi")) {
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
static int up_truncate(const char *path, off_t size,
                       struct fuse_file_info *fi) {
    if (!fuse_utils::check_null(path, "up_truncate: path")) {
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
static struct fuse_operations up_oper = {
    .getattr    = up_getattr,
    .mkdir      = up_mkdir,
    .unlink     = up_unlink,
    .rmdir      = up_rmdir,
    .rename     = up_rename,
    .truncate   = up_truncate,
    .open       = up_open,
    .read       = up_read,
    .write      = up_write,
    .release    = up_release,
    .readdir    = up_readdir,
    .create     = up_create,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mount_point> [fuse_options]\n", argv[0]);
        fprintf(stderr, "Example: %s /tmp/source /mnt/fuse -f\n", argv[0]);
        fprintf(stderr, "\nUppercase Filesystem:\n");
        fprintf(stderr, "  - Files are stored in original case on disk\n");
        fprintf(stderr, "  - All text is converted to UPPERCASE when read through FUSE\n");
        fprintf(stderr, "  - Writes are stored as-is (no conversion)\n");
        return 1;
    }

    root_path = argv[1];

    struct stat st;
    if (stat(root_path.c_str(), &st) == -1) {
        fprintf(stderr, "Error: Source directory '%s' does not exist\n", root_path.c_str());
        return 1;
    }

    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: '%s' is not a directory\n", root_path.c_str());
        return 1;
    }

    fprintf(stderr, "Mounting uppercase FS: %s -> %s\n", 
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

    int ret = fuse_main(fuse_argc, fuse_argv, &up_oper, nullptr);

    free(fuse_argv);
    return ret;
}