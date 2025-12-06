#define FUSE_USE_VERSION 35
#include <fuse3/fuse.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>

static std::string root;

// Получить текущее время в формате [YYYY-MM-DD HH:MM:SS]
static std::string get_timestamp() {
    time_t now = time(nullptr);
    struct tm *t = localtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
    return std::string(buf);
}

// Логирование операций
static void log_operation(const char *op, const char *path, int result) {
    fprintf(stderr, "[%s] %s: %s (result: %d)\n", 
            get_timestamp().c_str(), op, path, result);
}

// Преобразование пути из FUSE в реальную ФС
static std::string translate(const char *path) {
    if (strcmp(path, "/") == 0)
        return root;
    return root + path;
}

// getattr - получение атрибутов файла
static int pt_getattr(const char *path, struct stat *stbuf,
                      struct fuse_file_info *fi) {
    std::string p = translate(path);
    int res;

    if (fi != nullptr && fi->fh > 0)
        res = fstat(fi->fh, stbuf);
    else
        res = lstat(p.c_str(), stbuf);

    int ret = res == -1 ? -errno : 0;
    log_operation("GETATTR", path, ret);
    return ret;
}

// open - открытие файла
static int pt_open(const char *path, struct fuse_file_info *fi) {
    std::string p = translate(path);
    int fd = open(p.c_str(), fi->flags);
    
    int ret = 0;
    if (fd == -1) {
        ret = -errno;
    } else {
        fi->fh = fd;
    }
    
    log_operation("OPEN", path, ret);
    return ret;
}

// read - чтение из файла
static int pt_read(const char *path, char *buf, size_t size,
                   off_t offset, struct fuse_file_info *fi) {
    int fd = fi->fh;
    int res = pread(fd, buf, size, offset);
    
    int ret = res == -1 ? -errno : res;
    log_operation("READ", path, ret);
    return ret;
}

// write - запись в файл
static int pt_write(const char *path, const char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi) {
    int fd = fi->fh;
    int res = pwrite(fd, buf, size, offset);
    
    int ret = res == -1 ? -errno : res;
    log_operation("WRITE", path, ret);
    return ret;
}

// release - закрытие файла
static int pt_release(const char *path, struct fuse_file_info *fi) {
    close(fi->fh);
    log_operation("RELEASE", path, 0);
    return 0;
}

// mkdir - создание директории
static int pt_mkdir(const char *path, mode_t mode) {
    std::string p = translate(path);
    int res = mkdir(p.c_str(), mode);
    
    int ret = res == -1 ? -errno : 0;
    log_operation("MKDIR", path, ret);
    return ret;
}

// rmdir - удаление директории
static int pt_rmdir(const char *path) {
    std::string p = translate(path);
    int res = rmdir(p.c_str());
    
    int ret = res == -1 ? -errno : 0;
    log_operation("RMDIR", path, ret);
    return ret;
}

// unlink - удаление файла
static int pt_unlink(const char *path) {
    std::string p = translate(path);
    int res = unlink(p.c_str());
    
    int ret = res == -1 ? -errno : 0;
    log_operation("UNLINK", path, ret);
    return ret;
}

// rename - переименование/перемещение файла
static int pt_rename(const char *from, const char *to, unsigned int flags) {
    std::string f = translate(from);
    std::string t = translate(to);
    int res = rename(f.c_str(), t.c_str());
    
    int ret = res == -1 ? -errno : 0;
    log_operation("RENAME", from, ret);
    return ret;
}

// readdir - чтение содержимого директории
static int pt_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags) {
    std::string p = translate(path);

    DIR *dp = opendir(p.c_str());
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

// create - создание нового файла
static int pt_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    std::string p = translate(path);
    int fd = open(p.c_str(), fi->flags, mode);
    
    int ret = 0;
    if (fd == -1) {
        ret = -errno;
    } else {
        fi->fh = fd;
    }
    
    log_operation("CREATE", path, ret);
    return ret;
}

// truncate - изменение размера файла
static int pt_truncate(const char *path, off_t size,
                       struct fuse_file_info *fi) {
    int res;
    if (fi != nullptr && fi->fh > 0)
        res = ftruncate(fi->fh, size);
    else {
        std::string p = translate(path);
        res = truncate(p.c_str(), size);
    }
    
    int ret = res == -1 ? -errno : 0;
    log_operation("TRUNCATE", path, ret);
    return ret;
}

// chmod - изменение прав доступа
static int pt_chmod(const char *path, mode_t mode, struct fuse_file_info *fi) {
    std::string p = translate(path);
    int res = chmod(p.c_str(), mode);
    
    int ret = res == -1 ? -errno : 0;
    log_operation("CHMOD", path, ret);
    return ret;
}

// chown - изменение владельца
static int pt_chown(const char *path, uid_t uid, gid_t gid,
                    struct fuse_file_info *fi) {
    std::string p = translate(path);
    int res = lchown(p.c_str(), uid, gid);
    
    int ret = res == -1 ? -errno : 0;
    log_operation("CHOWN", path, ret);
    return ret;
}

// utimens - изменение временных меток
static int pt_utimens(const char *path, const struct timespec ts[2],
                      struct fuse_file_info *fi) {
    std::string p = translate(path);
    int res = utimensat(0, p.c_str(), ts, AT_SYMLINK_NOFOLLOW);
    
    int ret = res == -1 ? -errno : 0;
    log_operation("UTIMENS", path, ret);
    return ret;
}

// Структура операций FUSE
static struct fuse_operations pt_oper = {
    .getattr    = pt_getattr,
    .mkdir      = pt_mkdir,
    .unlink     = pt_unlink,
    .rmdir      = pt_rmdir,
    .rename     = pt_rename,
    .chmod      = pt_chmod,
    .chown      = pt_chown,
    .truncate   = pt_truncate,
    .open       = pt_open,
    .read       = pt_read,
    .write      = pt_write,
    .release    = pt_release,
    .readdir    = pt_readdir,
    .create     = pt_create,
    .utimens    = pt_utimens,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mount_point> [fuse_options]\n", argv[0]);
        fprintf(stderr, "Example: %s /tmp/source /mnt/fuse -f\n", argv[0]);
        return 1;
    }

    // Сохраняем путь к source директории
    root = argv[1];
    
    // Проверяем существование директории
    struct stat st;
    if (stat(root.c_str(), &st) == -1) {
        fprintf(stderr, "Error: Source directory '%s' does not exist\n", root.c_str());
        return 1;
    }
    
    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: '%s' is not a directory\n", root.c_str());
        return 1;
    }

    fprintf(stderr, "Mounting passthrough FS: %s -> %s\n", root.c_str(), argv[2]);

    // Передаем FUSE только mountpoint и опции (убираем source_dir)
    int fuse_argc = argc - 1;
    char **fuse_argv = (char**)malloc(sizeof(char*) * fuse_argc);
    fuse_argv[0] = argv[0];
    for (int i = 2; i < argc; i++) {
        fuse_argv[i-1] = argv[i];
    }

    int ret = fuse_main(fuse_argc, fuse_argv, &pt_oper, nullptr);
    
    free(fuse_argv);
    return ret;
}
