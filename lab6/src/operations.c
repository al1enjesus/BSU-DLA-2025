// src/operations.c
#define _XOPEN_SOURCE 700 
#define _GNU_SOURCE 
#define FUSE_USE_VERSION 26 

// --- 1. Системные и POSIX типы ---
#include <sys/stat.h>   
#include <sys/types.h>
#include <time.h>       // struct timespec
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>      // <-- Добавлен для errno
#include <fcntl.h>
#include <string.h>
#include <dirent.h>

// --- 2. FUSE ---
#include <fuse.h>       

#include "operations.h"

/**
 * @brief Получение метаданных файла (аналог stat/lstat). FUSE 2.x сигнатура.
 */
int my_getattr(const char *path, struct stat *stbuf) { 
    char fullpath[PATH_MAX_LEN];
    int res = 0;

    get_full_path(fullpath, path);

    // Используем lstat для получения информации, включая символические ссылки
    if (lstat(fullpath, stbuf) == -1) {
        res = -errno;
    }

    log_operation("GETATTR", path, res);
    return res;
}

/**
 * @brief Чтение содержимого директории (аналог readdir). FUSE 2.x сигнатура.
 */
int my_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    char fullpath[PATH_MAX_LEN];
    DIR *dp;
    struct dirent *de;
    int res = 0;
    
    // Подавляем предупреждение о неиспользуемом аргументе
    (void) fi; 

    get_full_path(fullpath, path);

    dp = opendir(fullpath);
    if (dp == NULL) {
        res = -errno;
        log_operation("READDIR", path, res);
        return res;
    }

    // Игнорируем offset, просто читаем все записи
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12; // Тип файла для filler

        // Вызов filler с 4 аргументами (FUSE 2.x совместимый)
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

    get_full_path(fullpath, path);

    res = open(fullpath, fi->flags);
    if (res == -1) {
        res = -errno;
        log_operation("OPEN", path, res);
        return res;
    }
    
    fi->fh = res;

    log_operation("OPEN", path, 0);
    return 0;
}

/**
 * @brief Чтение данных из файла (аналог pread).
 */
int my_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    int res;

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
int my_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    int res;

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

    get_full_path(fullpath, path);

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
    int res = 0;

    get_full_path(fullpath, path);

    if (unlink(fullpath) == -1) {
        res = -errno;
    }

    log_operation("UNLINK", path, res);
    return res;
}

/**
 * @brief Создание директории (аналог mkdir).
 */
int my_mkdir(const char *path, mode_t mode) {
    char fullpath[PATH_MAX_LEN];
    int res = 0;

    get_full_path(fullpath, path);

    if (mkdir(fullpath, mode) == -1) {
        res = -errno;
    }

    log_operation("MKDIR", path, res);
    return res;
}

/**
 * @brief Удаление директории (аналог rmdir).
 */
int my_rmdir(const char *path) {
    char fullpath[PATH_MAX_LEN];
    int res = 0;

    get_full_path(fullpath, path);

    if (rmdir(fullpath) == -1) {
        res = -errno;
    }

    log_operation("RMDIR", path, res);
    return res;
}

// Добавляем обязательный метод release для закрытия файлового дескриптора
int my_release(const char *path, struct fuse_file_info *fi) {
    (void) path; // Unused
    int res = close(fi->fh);
    if (res == -1) {
        res = -errno;
    }
    log_operation("RELEASE", path, res);
    return res;
}