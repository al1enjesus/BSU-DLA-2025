#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <limits.h>

// Глобальная переменная для хранения базового пути
static char* base_path = NULL;

/**
 * Логирование операций с временной меткой
 */
static void log_operation(const char* op, const char* path, int result) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
    fprintf(stderr, "[%s] %s: %s (result: %d)\n", timestamp, op, path, result);
}

/**
 * Построение полного пути из базового пути и относительного
 */
static void get_full_path(char* full_path, const char* path) {
    strcpy(full_path, base_path);
    strncat(full_path, path, PATH_MAX - strlen(base_path) - 1);
}

/**
 * Получение атрибутов файла
 */
static int passthrough_getattr(const char* path, struct stat* stbuf,
    struct fuse_file_info* fi) {
    (void)fi;
    char full_path[PATH_MAX];
    get_full_path(full_path, path);

    int res = lstat(full_path, stbuf);
    log_operation("GETATTR", path, res == 0 ? 0 : -errno);

    if (res == -1)
        return -errno;

    return 0;
}

/**
 * Чтение содержимого директории
 */
static int passthrough_readdir(const char* path, void* buf,
    fuse_fill_dir_t filler, off_t offset,
    struct fuse_file_info* fi,
    enum fuse_readdir_flags flags) {
    (void)offset;
    (void)fi;
    (void)flags;

    char full_path[PATH_MAX];
    get_full_path(full_path, path);

    DIR* dp = opendir(full_path);
    if (dp == NULL) {
        log_operation("READDIR", path, -errno);
        return -errno;
    }

    struct dirent* de;
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
    return 0;
}

/**
 * Открытие файла
 */
static int passthrough_open(const char* path, struct fuse_file_info* fi) {
    char full_path[PATH_MAX];
    get_full_path(full_path, path);

    int fd = open(full_path, fi->flags);
    if (fd == -1) {
        log_operation("OPEN", path, -errno);
        return -errno;
    }

    close(fd);
    log_operation("OPEN", path, 0);
    return 0;
}

/**
 * Чтение данных из файла
 */
static int passthrough_read(const char* path, char* buf, size_t size,
    off_t offset, struct fuse_file_info* fi) {
    (void)fi;
    char full_path[PATH_MAX];
    get_full_path(full_path, path);

    int fd = open(full_path, O_RDONLY);
    if (fd == -1) {
        log_operation("READ", path, -errno);
        return -errno;
    }

    int res = pread(fd, buf, size, offset);
    if (res == -1)
        res = -errno;

    close(fd);
    log_operation("READ", path, res);
    return res;
}

/**
 * Запись данных в файл
 */
static int passthrough_write(const char* path, const char* buf, size_t size,
    off_t offset, struct fuse_file_info* fi) {
    (void)fi;
    char full_path[PATH_MAX];
    get_full_path(full_path, path);

    int fd = open(full_path, O_WRONLY);
    if (fd == -1) {
        log_operation("WRITE", path, -errno);
        return -errno;
    }

    int res = pwrite(fd, buf, size, offset);
    if (res == -1)
        res = -errno;

    close(fd);
    log_operation("WRITE", path, res);
    return res;
}

/**
 * Создание файла
 */
static int passthrough_create(const char* path, mode_t mode,
    struct fuse_file_info* fi) {
    char full_path[PATH_MAX];
    get_full_path(full_path, path);

    int fd = open(full_path, fi->flags, mode);
    if (fd == -1) {
        log_operation("CREATE", path, -errno);
        return -errno;
    }

    fi->fh = fd;
    close(fd);
    log_operation("CREATE", path, 0);
    return 0;
}

/**
 * Удаление файла
 */
static int passthrough_unlink(const char* path) {
    char full_path[PATH_MAX];
    get_full_path(full_path, path);

    int res = unlink(full_path);
    if (res == -1) {
        log_operation("UNLINK", path, -errno);
        return -errno;
    }

    log_operation("UNLINK", path, 0);
    return 0;
}

/**
 * Создание директории
 */
static int passthrough_mkdir(const char* path, mode_t mode) {
    char full_path[PATH_MAX];
    get_full_path(full_path, path);

    int res = mkdir(full_path, mode);
    if (res == -1) {
        log_operation("MKDIR", path, -errno);
        return -errno;
    }

    log_operation("MKDIR", path, 0);
    return 0;
}

/**
 * Удаление директории
 */
static int passthrough_rmdir(const char* path) {
    char full_path[PATH_MAX];
    get_full_path(full_path, path);

    int res = rmdir(full_path);
    if (res == -1) {
        log_operation("RMDIR", path, -errno);
        return -errno;
    }

    log_operation("RMDIR", path, 0);
    return 0;
}

/**
 * Таблица операций FUSE
 */
static struct fuse_operations passthrough_oper = {
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

/**
 * Главная функция
 */
int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mount_point> [fuse_options]\n", argv[0]);
        return 1;
    }

    base_path = realpath(argv[1], NULL);
    if (base_path == NULL) {
        perror("realpath");
        return 1;
    }

    // Удаляем первый аргумент (source_dir) для FUSE
    for (int i = 1; i < argc - 1; i++) {
        argv[i] = argv[i + 1];
    }
    argc--;

    fprintf(stderr, "Mounting %s to %s\n", base_path, argv[1]);

    int ret = fuse_main(argc, argv, &passthrough_oper, NULL);

    free(base_path);
    return ret;
}