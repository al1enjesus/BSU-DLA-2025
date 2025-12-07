/*
 * operations.c
 *
 * Реализация FUSE операций для трёх режимов: passthrough, rot13, upper.
 *
 * Внимание:
 *  - g_base_path хранит исходную директорию (source) — указывается пользователем.
 *  - g_mode — режим работы FS.
 */

#define FUSE_USE_VERSION 31

#include "operations.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <linux/limits.h>

#include <time.h>

/* Внешние утилиты и логирование */
char *g_base_path = NULL;
fs_mode_t g_mode = MODE_PASSTHROUGH;

/* Prototypes for utils (from utils.c) */
int normalize_join(const char *base, const char *rel, char *out, size_t out_sz);
void log_operation(const char *op, const char *path, int result);
void rot13_transform_inplace(char *buf, size_t size);
void uppercase_inplace(char *buf, size_t size);

/* Initialize global base and mode. Возвращает 0 при успехе, -1 при ошибке */
int fs_init(const char *base, fs_mode_t mode) {
    if (!base) return -EINVAL;
    char *real = realpath(base, NULL);
    if (!real) {
        /* base may not exist */
        perror("realpath(base)");
        return -errno;
    }
    g_base_path = real;
    g_mode = mode;
    return 0;
}

/* Helper: convert fuse path -> full underlying path, with security checks */
static int get_full_path(const char *path, char *out, size_t out_sz) {
    /* delegate to normalize_join which ensures no traversal above base */
    int rc = normalize_join(g_base_path, path, out, out_sz);
    if (rc != 0) return rc;
    return 0;
}

/*
 * getattr - получить атрибуты файла (аналог stat)
 * Вызывается при: ls, stat, и перед большинством операций
 */
int fs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    char full[PATH_MAX];
    int rc = get_full_path(path, full, sizeof(full));
    if (rc != 0) {
        log_operation("GETATTR", path, rc);
        return rc;
    }

    int res = lstat(full, stbuf);
    if (res == -1) {
        int err = -errno;
        log_operation("GETATTR", path, err);
        return err;
    }

    log_operation("GETATTR", path, 0);
    return 0;
}

/*
 * readdir - прочитать содержимое директории
 */
int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
               off_t offset, struct fuse_file_info *fi,
               enum fuse_readdir_flags flags) {
    (void) offset; (void) fi; (void) flags;
    char full[PATH_MAX];
    int rc = get_full_path(path, full, sizeof(full));
    if (rc != 0) {
        log_operation("READDIR", path, rc);
        return rc;
    }

    DIR *dp = opendir(full);
    if (!dp) {
        int err = -errno;
        log_operation("READDIR", path, err);
        return err;
    }

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        /* filler returns non-zero to stop */
        if (filler(buf, de->d_name, &st, 0, 0)) break;
    }

    closedir(dp);
    log_operation("READDIR", path, 0);
    return 0;
}

/*
 * open - открыть файл (проверка прав)
 * Не оставляем файловый дескриптор в fi->fh, т.к. мы открываем/закрываем в read/write.
 */
int fs_open(const char *path, struct fuse_file_info *fi) {
    char full[PATH_MAX];
    int rc = get_full_path(path, full, sizeof(full));
    if (rc != 0) {
        log_operation("OPEN", path, rc);
        return rc;
    }

    int fd = open(full, fi->flags);
    if (fd == -1) {
        int err = -errno;
        log_operation("OPEN", path, err);
        return err;
    }

    close(fd);
    log_operation("OPEN", path, 0);
    return 0;
}

/*
 * read - прочитать данные из файла
 * В режиме:
 *  - passthrough: возвращаем как есть
 *  - rot13: читаем зашифрованные данные с диска и расшифровываем (ROT13) перед возвратом
 *  - upper: читаем как есть, преобразуем в верхний регистр перед возвратом
 */
int fs_read(const char *path, char *buf, size_t size, off_t offset,
            struct fuse_file_info *fi) {
    (void) fi;
    char full[PATH_MAX];
    int rc = get_full_path(path, full, sizeof(full));
    if (rc != 0) {
        log_operation("READ", path, rc);
        return rc;
    }

    int fd = open(full, O_RDONLY);
    if (fd == -1) {
        int err = -errno;
        log_operation("READ", path, err);
        return err;
    }

    ssize_t res = pread(fd, buf, size, offset);
    if (res == -1) {
        int err = -errno;
        close(fd);
        log_operation("READ", path, err);
        return err;
    }

    /* apply transformations in-place */
    if (g_mode == MODE_ROT13 && res > 0) {
        rot13_transform_inplace(buf, (size_t)res);
    } else if (g_mode == MODE_UPPER && res > 0) {
        uppercase_inplace(buf, (size_t)res);
    }

    close(fd);
    log_operation("READ", path, (int)res);
    return (int)res;
}

/*
 * write - записать данные в файл
 * В режиме:
 *  - passthrough: записать как есть
 *  - rot13: зашифровать буфер и записать на диск (файлы на диске должны быть зашифрованы)
 *  - upper: записываем как есть
 *
 * Возвращаем кол-во записанных байт или отрицательный errno.
 */
int fs_write(const char *path, const char *buf, size_t size,
             off_t offset, struct fuse_file_info *fi) {
    (void) fi;
    char full[PATH_MAX];
    int rc = get_full_path(path, full, sizeof(full));
    if (rc != 0) {
        log_operation("WRITE", path, rc);
        return rc;
    }

    /* Для rot13 создаём временную копию и шифруем её */
    char *tmpbuf = NULL;
    const char *write_buf = buf;
    if (g_mode == MODE_ROT13 && size > 0) {
        tmpbuf = malloc(size);
        if (!tmpbuf) {
            log_operation("WRITE", path, -ENOMEM);
            return -ENOMEM;
        }
        memcpy(tmpbuf, buf, size);
        rot13_transform_inplace(tmpbuf, size);
        write_buf = tmpbuf;
    }

    int fd = open(full, O_WRONLY);
    if (fd == -1) {
        /* Try to create file if not exist (common for echo > file) */
        if (errno == ENOENT) {
            fd = open(full, O_WRONLY | O_CREAT, 0644);
        }
    }
    if (fd == -1) {
        int err = -errno;
        if (tmpbuf) free(tmpbuf);
        log_operation("WRITE", path, err);
        return err;
    }

    ssize_t res = pwrite(fd, write_buf, size, offset);
    if (res == -1) {
        res = -errno;
    }

    close(fd);
    if (tmpbuf) free(tmpbuf);
    log_operation("WRITE", path, (int)res);
    return (int)res;
}

/*
 * create - создать новый файл
 * В режиме rot13 — создаём пустой файл (содержимое будет писаться зашифрованным)
 */
int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void) fi;
    char full[PATH_MAX];
    int rc = get_full_path(path, full, sizeof(full));
    if (rc != 0) {
        log_operation("CREATE", path, rc);
        return rc;
    }

    int fd = creat(full, mode);
    if (fd == -1) {
        int err = -errno;
        log_operation("CREATE", path, err);
        return err;
    }

    close(fd);
    log_operation("CREATE", path, 0);
    return 0;
}

/*
 * unlink - удалить файл
 */
int fs_unlink(const char *path) {
    char full[PATH_MAX];
    int rc = get_full_path(path, full, sizeof(full));
    if (rc != 0) {
        log_operation("UNLINK", path, rc);
        return rc;
    }

    int res = unlink(full);
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
int fs_mkdir(const char *path, mode_t mode) {
    char full[PATH_MAX];
    int rc = get_full_path(path, full, sizeof(full));
    if (rc != 0) {
        log_operation("MKDIR", path, rc);
        return rc;
    }

    int res = mkdir(full, mode);
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
int fs_rmdir(const char *path) {
    char full[PATH_MAX];
    int rc = get_full_path(path, full, sizeof(full));
    if (rc != 0) {
        log_operation("RMDIR", path, rc);
        return rc;
    }

    int res = rmdir(full);
    if (res == -1) {
        int err = -errno;
        log_operation("RMDIR", path, err);
        return err;
    }

    log_operation("RMDIR", path, 0);
    return 0;
}

/* destroy called on exit */
void fs_destroy(void *private_data) {
    (void) private_data;
    log_operation("DESTROY", "/", 0);
    if (g_base_path) {
        free(g_base_path);
        g_base_path = NULL;
    }
}
