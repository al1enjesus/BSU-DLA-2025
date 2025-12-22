/*
 * Monitoring FUSE filesystem - Задание C (Вариант 1)
 *
 * Passthrough FS с подсчетом статистики обращений.
 * Виртуальный файл /.stats показывает статистику.
 * Использование: ./monitoring <source_dir> <mount_point>
 */

#define FUSE_USE_VERSION 31

#include <stddef.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>

/* Глобальная переменная для хранения базовой директории */
static char *base_path = NULL;

/* Статистика */
static unsigned long long reads = 0;
static unsigned long long writes = 0;
static unsigned long long opens = 0;
static unsigned long long creates = 0;
static unsigned long long unlinks = 0;
static unsigned long long bytes_read = 0;
static unsigned long long bytes_written = 0;

/* Вспомогательная функция: получить текущий timestamp для логов */
static void log_operation(const char *op, const char *path, int result) {
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    fprintf(stderr, "[%s] %s: %s (result: %d)\n", timestamp, op, path, result);
}

/* Построить полный путь: base_path + relative_path */
static void get_full_path(char *fullpath, const char *path) {
    strcpy(fullpath, base_path);
    strcat(fullpath, path);
}

/*
 * getattr - получить атрибуты файла
 */
static int monitoring_getattr(const char *path, struct stat *stbuf,
                              struct fuse_file_info *fi) {
    (void) fi;

    if (strcmp(path, "/.stats") == 0) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = 1024; // approximate
        log_operation("GETATTR", path, 0);
        return 0;
    }

    char fullpath[1024];
    get_full_path(fullpath, path);

    int res = lstat(fullpath, stbuf);
    log_operation("GETATTR", path, res);

    if (res == -1)
        return -errno;

    return 0;
}

/*
 * readdir - прочитать содержимое директории
 */
static int monitoring_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                              off_t offset, struct fuse_file_info *fi,
                              enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    char fullpath[1024];
    get_full_path(fullpath, path);

    DIR *dp = opendir(fullpath);
    if (dp == NULL) {
        log_operation("READDIR", path, -errno);
        return -errno;
    }

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    filler(buf, ".stats", NULL, 0, 0);

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
    return 0;
}

/*
 * open - открыть файл
 */
static int monitoring_open(const char *path, struct fuse_file_info *fi) {
    if (strcmp(path, "/.stats") == 0) {
        if ((fi->flags & O_ACCMODE) != O_RDONLY) {
            log_operation("OPEN", path, -EACCES);
            return -EACCES;
        }
        opens++;
        log_operation("OPEN", path, 0);
        return 0;
    }

    char fullpath[1024];
    get_full_path(fullpath, path);

    int fd = open(fullpath, fi->flags);
    if (fd == -1) {
        log_operation("OPEN", path, -errno);
        return -errno;
    }

    close(fd);
    opens++;
    log_operation("OPEN", path, 0);
    return 0;
}

/*
 * read - прочитать данные из файла
 */
static int monitoring_read(const char *path, char *buf, size_t size, off_t offset,
                           struct fuse_file_info *fi) {
    (void) fi;

    if (strcmp(path, "/.stats") == 0) {
        char stats[1024];
        sprintf(stats, "reads: %llu\nwrites: %llu\nopens: %llu\ncreates: %llu\nunlinks: %llu\nbytes_read: %llu\nbytes_written: %llu\n",
                reads, writes, opens, creates, unlinks, bytes_read, bytes_written);

        size_t len = strlen(stats);
        if (offset >= len) return 0;
        size_t to_copy = size;
        if (offset + size > len) to_copy = len - offset;
        memcpy(buf, stats + offset, to_copy);

        reads++;
        bytes_read += to_copy;

        time_t now = time(NULL);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(stderr, "[%s] READ: %s (%zu bytes at offset %ld, result: %d)\n",
                timestamp, path, size, offset, (int)to_copy);

        return to_copy;
    }

    char fullpath[1024];
    get_full_path(fullpath, path);

    int fd = open(fullpath, O_RDONLY);
    if (fd == -1) {
        log_operation("READ", path, -errno);
        return -errno;
    }

    int res = pread(fd, buf, size, offset);
    if (res == -1)
        res = -errno;
    else {
        reads++;
        bytes_read += res;
    }

    close(fd);

    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(stderr, "[%s] READ: %s (%zu bytes at offset %ld, result: %d)\n",
            timestamp, path, size, offset, res);

    return res;
}

/*
 * write - записать данные в файл
 */
static int monitoring_write(const char *path, const char *buf, size_t size,
                            off_t offset, struct fuse_file_info *fi) {
    (void) fi;
    char fullpath[1024];
    get_full_path(fullpath, path);

    int fd = open(fullpath, O_WRONLY);
    if (fd == -1) {
        log_operation("WRITE", path, -errno);
        return -errno;
    }

    int res = pwrite(fd, buf, size, offset);
    if (res == -1)
        res = -errno;
    else {
        writes++;
        bytes_written += res;
    }

    close(fd);

    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(stderr, "[%s] WRITE: %s (%zu bytes at offset %ld, result: %d)\n",
            timestamp, path, size, offset, res);

    return res;
}

/*
 * create - создать новый файл
 */
static int monitoring_create(const char *path, mode_t mode,
                             struct fuse_file_info *fi) {
    char fullpath[1024];
    get_full_path(fullpath, path);

    int fd = creat(fullpath, mode);
    if (fd == -1) {
        log_operation("CREATE", path, -errno);
        return -errno;
    }

    fi->fh = fd;
    close(fd);

    creates++;
    log_operation("CREATE", path, 0);
    return 0;
}

/*
 * unlink - удалить файл
 */
static int monitoring_unlink(const char *path) {
    char fullpath[1024];
    get_full_path(fullpath, path);

    int res = unlink(fullpath);
    log_operation("UNLINK", path, res);

    if (res == -1)
        return -errno;

    unlinks++;
    return 0;
}

/*
 * mkdir - создать директорию
 */
static int monitoring_mkdir(const char *path, mode_t mode) {
    char fullpath[1024];
    get_full_path(fullpath, path);

    int res = mkdir(fullpath, mode);
    log_operation("MKDIR", path, res);

    if (res == -1)
        return -errno;

    return 0;
}

/*
 * rmdir - удалить директорию
 */
static int monitoring_rmdir(const char *path) {
    char fullpath[1024];
    get_full_path(fullpath, path);

    int res = rmdir(fullpath);
    log_operation("RMDIR", path, res);

    if (res == -1)
        return -errno;

    return 0;
}

/* Структура с указателями на все операции */
static struct fuse_operations monitoring_oper = {
    .getattr    = monitoring_getattr,
    .readdir    = monitoring_readdir,
    .open       = monitoring_open,
    .read       = monitoring_read,
    .write      = monitoring_write,
    .create     = monitoring_create,
    .unlink     = monitoring_unlink,
    .mkdir      = monitoring_mkdir,
    .rmdir      = monitoring_rmdir,
};

int main(int argc, char *argv[]) {
    /* Проверка аргументов */
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mount_point> [fuse_options]\n", argv[0]);
        fprintf(stderr, "Example: %s /tmp/source /mnt/fuse -f\n", argv[0]);
        return 1;
    }

    /* Сохранить базовую директорию */
    base_path = realpath(argv[1], NULL);
    if (base_path == NULL) {
        perror("realpath");
        return 1;
    }

    fprintf(stderr, "Monitoring FS: %s at %s\n", base_path, argv[2]);

    /* Запустить FUSE */
    int fuse_argc = argc - 1;
    char **fuse_argv = malloc(sizeof(char*) * fuse_argc);
    fuse_argv[0] = argv[0];
    for (int i = 2; i < argc; i++) {
        fuse_argv[i-1] = argv[i];
    }

    int ret = fuse_main(fuse_argc, fuse_argv, &monitoring_oper, NULL);

    free(fuse_argv);
    free(base_path);

    return ret;
}