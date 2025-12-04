#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>

/* Глобальная переменная для хранения базовой директории */
static char *base_path = NULL;

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
    if (strcmp(path, "/") != 0) {
        strcat(fullpath, path);
    }
}

/*
 * getattr - получить атрибуты файла (аналог stat)
 */
static int passthrough_getattr(const char *path, struct stat *stbuf,
                                struct fuse_file_info *fi) {
    (void) fi;
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
static int passthrough_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                                off_t offset, struct fuse_file_info *fi,
                                enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    char fullpath[1024];
    get_full_path(fullpath, path);

    DIR *dp = opendir(fullpath);
    if (dp == NULL) {
        int res = -errno;
        log_operation("READDIR", path, res);
        return res;
    }

    struct dirent *de;
    int count = 0;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;

        if (filler(buf, de->d_name, &st, 0, 0))
            break;
        count++;
    }

    closedir(dp);
    log_operation("READDIR", path, count);
    return 0;
}

/*
 * open - открыть файл
 */
static int passthrough_open(const char *path, struct fuse_file_info *fi) {
    char fullpath[1024];
    get_full_path(fullpath, path);

    int res = open(fullpath, fi->flags);
    if (res == -1) {
        int err = -errno;
        log_operation("OPEN", path, err);
        return err;
    }

    close(res);
    log_operation("OPEN", path, 0);
    return 0;
}

/*
 * read - прочитать данные из файла
 */
static int passthrough_read(const char *path, char *buf, size_t size, off_t offset,
                            struct fuse_file_info *fi) {
    (void) fi;
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
static int passthrough_write(const char *path, const char *buf, size_t size,
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
static int passthrough_create(const char *path, mode_t mode,
                               struct fuse_file_info *fi) {
    char fullpath[1024];
    get_full_path(fullpath, path);

    int res = creat(fullpath, mode);
    if (res == -1) {
        int err = -errno;
        log_operation("CREATE", path, err);
        return err;
    }

    close(res);
    log_operation("CREATE", path, 0);
    return 0;
}

/*
 * unlink - удалить файл
 */
static int passthrough_unlink(const char *path) {
    char fullpath[1024];
    get_full_path(fullpath, path);

    int res = unlink(fullpath);
    if (res == -1) {
        int err = -errno;
        log_operation("UNLINK", path, err);
        return err;
    }

    log_operation("UNLINK", path, 0);
    return 0;
}

/*
 * mkdir - создать директорию
 */
static int passthrough_mkdir(const char *path, mode_t mode) {
    char fullpath[1024];
    get_full_path(fullpath, path);

    int res = mkdir(fullpath, mode);
    if (res == -1) {
        int err = -errno;
        log_operation("MKDIR", path, err);
        return err;
    }

    log_operation("MKDIR", path, 0);
    return 0;
}

/*
 * rmdir - удалить директорию
 */
static int passthrough_rmdir(const char *path) {
    char fullpath[1024];
    get_full_path(fullpath, path);

    int res = rmdir(fullpath);
    if (res == -1) {
        int err = -errno;
        log_operation("RMDIR", path, err);
        return err;
    }

    log_operation("RMDIR", path, 0);
    return 0;
}

/* Структура с указателями на все операции */
static struct fuse_operations passthrough_oper = {
    .getattr    = passthrough_getattr,
    .readdir    = passthrough_readdir,
    .open       = passthrough_open,
    .read       = passthrough_read,
    .write      = passthrough_write,
    .create     = passthrough_create,
    .unlink     = passthrough_unlink,
    .mkdir      = passthrough_mkdir,
    .rmdir      = passthrough_rmdir,
};

int main(int argc, char *argv[]) {
    /* Проверка аргументов */
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mount_point> [fuse_options]\n", argv[0]);
        fprintf(stderr, "Example: %s /tmp/source /mnt/fuse -f\n", argv[0]);
        fprintf(stderr, "\nOptions:\n");
        fprintf(stderr, "  -f  foreground mode (see logs)\n");
        fprintf(stderr, "  -d  debug mode (detailed FUSE output)\n");
        fprintf(stderr, "  -s  single-threaded mode\n");
        return 1;
    }

    /* Сохранить базовую директорию */
    base_path = realpath(argv[1], NULL);
    if (base_path == NULL) {
        perror("realpath");
        fprintf(stderr, "Error: Source directory '%s' not found\n", argv[1]);
        return 1;
    }

    /* Проверить что это директория */
    struct stat st;
    if (stat(base_path, &st) == -1 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: '%s' is not a directory\n", argv[1]);
        free(base_path);
        return 1;
    }

    fprintf(stderr, "=== FUSE Passthrough Filesystem ===\n");
    fprintf(stderr, "Source: %s\n", base_path);
    fprintf(stderr, "Mount:  %s\n", argv[2]);
    fprintf(stderr, "To unmount: fusermount -u %s\n\n", argv[2]);

    /* Запустить FUSE */
    int fuse_argc = argc - 1;
    char **fuse_argv = malloc(sizeof(char*) * (fuse_argc + 1));
    fuse_argv[0] = argv[0];
    for (int i = 2; i < argc; i++) {
        fuse_argv[i-1] = argv[i];
    }
    fuse_argv[fuse_argc] = NULL;

    int ret = fuse_main(fuse_argc, fuse_argv, &passthrough_oper, NULL);

    free(fuse_argv);
    free(base_path);

    return ret;
}
