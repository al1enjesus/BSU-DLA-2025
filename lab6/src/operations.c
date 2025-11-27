#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#define FUSE_USE_VERSION 26

#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <fuse.h>

#include "operations.h"

/**
 * @brief Макрос для выполнения преобразования пути и проверки безопасности.
 * Возвращает ошибку, если путь слишком длинный или небезопасный.
 */
#define PREPARE_PATH(path, fullpath, res) \
    if ((res = get_full_path(fullpath, path)) != 0) { \
        return res; \
    } \
    if ((res = check_path_security(fullpath)) != 0) { \
        log_operation("SECURITY_BLOCK", path, res); \
        return res; \
    }

/**
 * @brief Получение метаданных файла (аналог stat/lstat).
 */
int my_getattr(const char *path, struct stat *stbuf) {
    char fullpath[PATH_MAX_LEN];
    int res;

    // 1. Преобразование пути и проверка безопасности
    PREPARE_PATH(path, fullpath, res);

    // 2. Выполнение операции
    if (lstat(fullpath, stbuf) == -1) {
        res = -errno;
    } else {
        res = 0;
    }

    log_operation("GETATTR", path, res);
    return res;
}

/**
 * @brief Чтение содержимого директории (аналог readdir).
 */
int my_readdir(const char *path, void *buf, fuse_fill_dir_t filler, 
               off_t offset, struct fuse_file_info *fi) {
    char fullpath[PATH_MAX_LEN];
    DIR *dp;
    struct dirent *de;
    int res;

    (void) offset;
    (void) fi;

    // 1. Преобразование пути и проверка безопасности
    PREPARE_PATH(path, fullpath, res);

    // 2. Открытие директории
    dp = opendir(fullpath);
    if (dp == NULL) {
        res = -errno;
        log_operation("READDIR", path, res);
        return res;
    }

    // 3. Читаем все записи директории
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        // FUSE 2.x требует, чтобы мы сами заполняли d_type в st_mode
        st.st_mode = de->d_type << 12; 

        // FUSE 2.x: filler принимает 4 аргумента
        if (filler(buf, de->d_name, &st, 0)) {
            closedir(dp);
            log_operation("READDIR", path, -ENOMEM);
            return -ENOMEM;
        }
    }

    closedir(dp);
    log_operation("READDIR", path, 0);
    return 0;
}

/**
 * @brief Открытие файла (аналог open).
 */
int my_open(const char *path, struct fuse_file_info *fi) {
    char fullpath[PATH_MAX_LEN];
    int res;

    // 1. Преобразование пути и проверка безопасности
    PREPARE_PATH(path, fullpath, res);

    // 2. Открытие реального файла
    res = open(fullpath, fi->flags);
    if (res == -1) {
        res = -errno;
        log_operation("OPEN", path, res);
        return res;
    }

    fi->fh = res;  // Сохраняем file descriptor
    log_operation("OPEN", path, 0);
    return 0;
}

/**
 * @brief Чтение данных из файла (аналог pread).
 */
int my_read(const char *path, char *buf, size_t size, off_t offset, 
            struct fuse_file_info *fi) {
    int res;
    (void)path; // Не используется, так как используем fi->fh

    res = pread(fi->fh, buf, size, offset);
    if (res == -1) {
        res = -errno;
    }

    log_operation("READ", path, res);
    return res;
}

/**
 * @brief Запись данных в файл (аналог pwrite).
 */
int my_write(const char *path, const char *buf, size_t size, off_t offset, 
             struct fuse_file_info *fi) {
    int res;
    (void)path; // Не используется, так как используем fi->fh

    res = pwrite(fi->fh, buf, size, offset);
    if (res == -1) {
        res = -errno;
    }

    log_operation("WRITE", path, res);
    return res;
}

/**
 * @brief Создание и открытие файла (аналог open с O_CREAT).
 */
int my_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char fullpath[PATH_MAX_LEN];
    int res;

    // 1. Преобразование пути и проверка безопасности
    PREPARE_PATH(path, fullpath, res);

    // 2. Создание и открытие файла
    res = open(fullpath, fi->flags, mode);
    if (res == -1) {
        res = -errno;
        log_operation("CREATE", path, res);
        return res;
    }

    fi->fh = res;
    log_operation("CREATE", path, 0);
    return 0;
}

/**
 * @brief Удаление файла (аналог unlink).
 */
int my_unlink(const char *path) {
    char fullpath[PATH_MAX_LEN];
    int res;

    // 1. Преобразование пути и проверка безопасности
    PREPARE_PATH(path, fullpath, res);

    // 2. Удаление
    if (unlink(fullpath) == -1) {
        res = -errno;
    } else {
        res = 0;
    }

    log_operation("UNLINK", path, res);
    return res;
}

/**
 * @brief Создание директории (аналог mkdir).
 */
int my_mkdir(const char *path, mode_t mode) {
    char fullpath[PATH_MAX_LEN];
    int res;

    // 1. Преобразование пути и проверка безопасности
    PREPARE_PATH(path, fullpath, res);

    // 2. Создание директории
    if (mkdir(fullpath, mode) == -1) {
        res = -errno;
    } else {
        res = 0;
    }

    log_operation("MKDIR", path, res);
    return res;
}

/**
 * @brief Удаление директории (аналог rmdir).
 */
int my_rmdir(const char *path) {
    char fullpath[PATH_MAX_LEN];
    int res;

    // 1. Преобразование пути и проверка безопасности
    PREPARE_PATH(path, fullpath, res);

    // 2. Удаление директории
    if (rmdir(fullpath) == -1) {
        res = -errno;
    } else {
        res = 0;
    }

    log_operation("RMDIR", path, res);
    return res;
}

/**
 * @brief Закрытие файла (аналог close).
 */
int my_release(const char *path, struct fuse_file_info *fi) {
    int res;
    (void)path; // Не используется, так как используем fi->fh

    res = close(fi->fh);
    if (res == -1) {
        res = -errno;
    }

    log_operation("RELEASE", path, res);
    return res;
}