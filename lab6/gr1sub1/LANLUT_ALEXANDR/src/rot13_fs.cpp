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

// ROT13 преобразование символа
static char rot13_char(char c) {
    if (c >= 'A' && c <= 'Z') {
        return 'A' + (c - 'A' + 13) % 26;
    }
    if (c >= 'a' && c <= 'z') {
        return 'a' + (c - 'a' + 13) % 26;
    }
    return c; // Не-буквы остаются без изменений
}

// ROT13 преобразование буфера
static void rot13_buffer(char *buf, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buf[i] = rot13_char(buf[i]);
    }
}

// Преобразование пути из FUSE в реальную ФС
static std::string translate(const char *path) {
    if (strcmp(path, "/") == 0)
        return root;
    return root + path;
}

// getattr - получение атрибутов файла
static int rot13_getattr(const char *path, struct stat *stbuf,
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
static int rot13_open(const char *path, struct fuse_file_info *fi) {
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

// read - чтение из файла с расшифровкой ROT13
static int rot13_read(const char *path, char *buf, size_t size,
                      off_t offset, struct fuse_file_info *fi) {
    int fd = fi->fh;
    int res = pread(fd, buf, size, offset);
    
    if (res > 0) {
        // Расшифровываем данные при чтении
        rot13_buffer(buf, res);
    }
    
    int ret = res == -1 ? -errno : res;
    log_operation("READ", path, ret);
    return ret;
}

// write - запись в файл с шифрованием ROT13
static int rot13_write(const char *path, const char *buf, size_t size,
                       off_t offset, struct fuse_file_info *fi) {
    int fd = fi->fh;
    
    // Создаем копию буфера для шифрования
    char *encrypted_buf = (char*)malloc(size);
    if (!encrypted_buf) {
        return -ENOMEM;
    }
    
    memcpy(encrypted_buf, buf, size);
    
    // Шифруем данные перед записью
    rot13_buffer(encrypted_buf, size);
    
    int res = pwrite(fd, encrypted_buf, size, offset);
    
    free(encrypted_buf);
    
    int ret = res == -1 ? -errno : res;
    log_operation("WRITE", path, ret);
    return ret;
}

// release - закрытие файла
static int rot13_release(const char *path, struct fuse_file_info *fi) {
    close(fi->fh);
    log_operation("RELEASE", path, 0);
    return 0;
}

// mkdir - создание директории
static int rot13_mkdir(const char *path, mode_t mode) {
    std::string p = translate(path);
    int res = mkdir(p.c_str(), mode);
    
    int ret = res == -1 ? -errno : 0;
    log_operation("MKDIR", path, ret);
    return ret;
}

// rmdir - удаление директории
static int rot13_rmdir(const char *path) {
    std::string p = translate(path);
    int res = rmdir(p.c_str());
    
    int ret = res == -1 ? -errno : 0;
    log_operation("RMDIR", path, ret);
    return ret;
}

// unlink - удаление файла
static int rot13_unlink(const char *path) {
    std::string p = translate(path);
    int res = unlink(p.c_str());
    
    int ret = res == -1 ? -errno : 0;
    log_operation("UNLINK", path, ret);
    return ret;
}

// rename - переименование/перемещение файла
static int rot13_rename(const char *from, const char *to, unsigned int flags) {
    std::string f = translate(from);
    std::string t = translate(to);
    int res = rename(f.c_str(), t.c_str());
    
    int ret = res == -1 ? -errno : 0;
    log_operation("RENAME", from, ret);
    return ret;
}

// readdir - чтение содержимого директории
static int rot13_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
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
static int rot13_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
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
static int rot13_truncate(const char *path, off_t size,
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
static int rot13_chmod(const char *path, mode_t mode, struct fuse_file_info *fi) {
    std::string p = translate(path);
    int res = chmod(p.c_str(), mode);
    
    int ret = res == -1 ? -errno : 0;
    log_operation("CHMOD", path, ret);
    return ret;
}

// chown - изменение владельца
static int rot13_chown(const char *path, uid_t uid, gid_t gid,
                       struct fuse_file_info *fi) {
    std::string p = translate(path);
    int res = lchown(p.c_str(), uid, gid);
    
    int ret = res == -1 ? -errno : 0;
    log_operation("CHOWN", path, ret);
    return ret;
}

// utimens - изменение временных меток
static int rot13_utimens(const char *path, const struct timespec ts[2],
                         struct fuse_file_info *fi) {
    std::string p = translate(path);
    int res = utimensat(0, p.c_str(), ts, AT_SYMLINK_NOFOLLOW);
    
    int ret = res == -1 ? -errno : 0;
    log_operation("UTIMENS", path, ret);
    return ret;
}

// Структура операций FUSE
static struct fuse_operations rot13_oper = {
    .getattr    = rot13_getattr,
    .mkdir      = rot13_mkdir,
    .unlink     = rot13_unlink,
    .rmdir      = rot13_rmdir,
    .rename     = rot13_rename,
    .chmod      = rot13_chmod,
    .chown      = rot13_chown,
    .truncate   = rot13_truncate,
    .open       = rot13_open,
    .read       = rot13_read,
    .write      = rot13_write,
    .release    = rot13_release,
    .readdir    = rot13_readdir,
    .create     = rot13_create,
    .utimens    = rot13_utimens,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mount_point> [fuse_options]\n", argv[0]);
        fprintf(stderr, "Example: %s /tmp/source /mnt/fuse -f\n", argv[0]);
        fprintf(stderr, "\nROT13 Encryption Filesystem:\n");
        fprintf(stderr, "  - Files are encrypted with ROT13 on disk\n");
        fprintf(stderr, "  - Automatically decrypted when read through FUSE\n");
        fprintf(stderr, "  - Automatically encrypted when written through FUSE\n");
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

    fprintf(stderr, "Mounting ROT13 encryption FS: %s -> %s\n", root.c_str(), argv[2]);
    fprintf(stderr, "All file contents will be encrypted with ROT13\n");

    // Передаем FUSE только mountpoint и опции (убираем source_dir)
    int fuse_argc = argc - 1;
    char **fuse_argv = (char**)malloc(sizeof(char*) * fuse_argc);
    fuse_argv[0] = argv[0];
    for (int i = 2; i < argc; i++) {
        fuse_argv[i-1] = argv[i];
    }

    int ret = fuse_main(fuse_argc, fuse_argv, &rot13_oper, nullptr);
    
    free(fuse_argv);
    return ret;
}
