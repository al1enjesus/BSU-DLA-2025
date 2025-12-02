#define _GNU_SOURCE
#include "operations.h"
#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "utils.h"
#include <sys/time.h>
#include <stdlib.h>

struct fs_config g_config = { .root = "/tmp/source", .mode = MODE_PASSTHROUGH };

/*
 * ensure_fullpath
 * ----------------
 * Вспомогательная функция, которая оборачивает `join_path` и приводит
 * возможную ошибку к отрицательному коду errno, подходящему для возврата
 * из FUSE-колбека. Возвращает 0 при успехе или отрицательный код ошибки.
 */
static int ensure_fullpath(char *fullpath, const char *path) {
    if (join_path(fullpath, path) != 0) return -EACCES;
    return 0;
}

/*
 * fs_getattr
 * ----------
 * Реализация getattr для FUSE. Вызывается ядром при запросе метаданных
 * для пути `path` (аналогично stat()).
 *
 * Действия:
 * - Формирует реальный путь на диске через ensure_fullpath.
 * - Вызывает lstat() для получения атрибутов.
 * - Логирует операцию и возвращает 0 или отрицательный errno.
 */
int fs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    char full[PATH_MAX_LEN];
    int rc = ensure_fullpath(full, path);
    if (rc) {
        log_op("GETATTR", path, rc);
        return rc;
    }
    int res = lstat(full, stbuf);
    int out = res == -1 ? -errno : 0;
    log_op("GETATTR", path, out);
    return out;
}

/*
 * fs_readdir
 * ---------
 * Реализация readdir для FUSE. Вызывается при чтении каталога (opendir/readdir).
 *
 * Действия:
 * - Формирует реальный путь к директории.
 * - Открывает директорию через opendir и в цикле вызывает filler для
 *   всех найденных имён (включая "." и "..").
 * - Возвращает 0 при успехе или отрицательный код ошибки.
 */
int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    (void) offset; (void) fi;
    char full[PATH_MAX_LEN];
    if (ensure_fullpath(full, path) != 0) {
        log_op("READDIR", path, -EACCES);
        return -EACCES;
    }
    DIR *dp = opendir(full);
    if (!dp) {
        int e = -errno;
        log_op("READDIR", path, e);
        return e;
    }
    struct dirent *de;
    /* Обязательно возвращаем '.' и '..' */
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    while ((de = readdir(dp)) != NULL) {
        filler(buf, de->d_name, NULL, 0, 0);
    }
    closedir(dp);
    log_op("READDIR", path, 0);
    return 0;
}

/*
 * fs_open
 * -------
 * Открывает файл для последующих операций read/write.
 *
 * Поведение:
 * - Формирует путь к реальному файлу.
 * - Вызывает open(full, fi->flags).
 * - Сохраняет файловый дескриптор в fi->fh для последующих операций.
 * - Возвращает 0 при успехе или отрицательный код ошибки.
 */
int fs_open(const char *path, struct fuse_file_info *fi) {
    char full[PATH_MAX_LEN];
    if (ensure_fullpath(full, path) != 0) {
        log_op("OPEN", path, -EACCES);
        return -EACCES;
    }
    int fd = open(full, fi->flags);
    if (fd == -1) {
        int e = -errno;
        log_op("OPEN", path, e);
        return e;
    }
    fi->fh = (uint64_t)fd;
    log_op("OPEN", path, 0);
    return 0;
}

/*
 * fs_read
 * -------
 * Читает `size` байт из файла, начиная с `offset`.
 *
 * Особенности реализации:
 * - Если файл был открыт ранее через fs_open, дескриптор хранится в fi->fh
 *   и используется для чтения.
 * - Иначе функция открывает файл самостоятельно и закрывает по завершении.
 * - После чтения, в зависимости от режима (`g_config.mode`) буфер может быть
 *   трансформирован: ROT13 или преобразование в верхний регистр.
 * - Возвращает количество прочитанных байт или отрицательный errno при ошибке.
 */
int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    int fd;
    if (fi) fd = (int)fi->fh;
    else {
        char full[PATH_MAX_LEN];
        if (ensure_fullpath(full, path) != 0) {
            log_op("READ", path, -EACCES);
            return -EACCES;
        }
        fd = open(full, O_RDONLY);
        if (fd == -1) {
            int e = -errno; log_op("READ", path, e); return e;
        }
    }
    ssize_t res = pread(fd, buf, size, offset);
    if (!fi) close(fd);
    if (res == -1) {
        int e = -errno; log_op("READ", path, e); return e;
    }
    /* Применяем трансформации на лету в зависимости от режима */
    if (g_config.mode == MODE_ROT13) rot13_buf(buf, res);
    else if (g_config.mode == MODE_UPPERCASE) strtoupper_buf(buf, res);

    log_op("READ", path, (int)res);
    return res;
}

/*
 * fs_write
 * --------
 * Записывает данные в файл: size байт из `buf` в позицию `offset`.
 *
 * Поведение:
 * - Если режим ROT13 — выполняет шифрование данных перед записью на диск.
 * - Для других режимов (passthrough, uppercase) запись выполняется без модификации
 *   (uppercase меняет только чтение).
 * - Если файл был открыт ранее, используется fi->fh, иначе функция открывает
 *   файл самостоятельно и закрывает его.
 * - Возвращает количество записанных байт или отрицательный errno при ошибке.
 */
int fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    int fd;
    if (fi) fd = (int)fi->fh;
    else {
        char full[PATH_MAX_LEN];
        if (ensure_fullpath(full, path) != 0) {
            log_op("WRITE", path, -EACCES);
            return -EACCES;
        }
        fd = open(full, O_WRONLY);
        if (fd == -1) { int e = -errno; log_op("WRITE", path, e); return e; }
    }
    /* Создаём локальную копию буфера, т.к. возможно требуется модификация */
    char *tmp = malloc(size);
    if (!tmp) { if (!fi) close(fd); return -ENOMEM; }
    memcpy(tmp, buf, size);
    if (g_config.mode == MODE_ROT13) rot13_buf(tmp, size);
    /* В режиме uppercase не модифицируем записываемые данные */
    ssize_t res = pwrite(fd, tmp, size, offset);
    free(tmp);
    if (!fi) close(fd);
    if (res == -1) { int e = -errno; log_op("WRITE", path, e); return e; }
    log_op("WRITE", path, (int)res);
    return res;
}

/*
 * fs_create
 * ---------
 * Создаёт новый файл с правами `mode` и открывает его. Сохраняет дескриптор
 * в fi->fh для последующих операций.
 *
 * Возвращает 0 при успехе или отрицательный errno при ошибке.
 */
int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char full[PATH_MAX_LEN];
    if (ensure_fullpath(full, path) != 0) {
        log_op("CREATE", path, -EACCES); return -EACCES;
    }
    int fd = open(full, fi->flags | O_CREAT, mode);
    if (fd == -1) { int e = -errno; log_op("CREATE", path, e); return e; }
    fi->fh = (uint64_t)fd;
    log_op("CREATE", path, 0);
    return 0;
}

/*
 * fs_unlink
 * ---------
 * Удаляет файл на реальном файловом разделе, соответствующий `path`.
 * Возвращает 0 при успехе или отрицательный errno при ошибке.
 */
int fs_unlink(const char *path) {
    char full[PATH_MAX_LEN];
    if (ensure_fullpath(full, path) != 0) { log_op("UNLINK", path, -EACCES); return -EACCES; }
    int res = unlink(full);
    int out = res == -1 ? -errno : 0;
    log_op("UNLINK", path, out);
    return out;
}

/*
 * fs_mkdir
 * -------
 * Создаёт директорию с правами `mode` по указанному пути внутри исходного
 * каталога. Возвращает 0 при успехе или отрицательный errno при ошибке.
 */
int fs_mkdir(const char *path, mode_t mode) {
    char full[PATH_MAX_LEN];
    if (ensure_fullpath(full, path) != 0) { log_op("MKDIR", path, -EACCES); return -EACCES; }
    int res = mkdir(full, mode);
    int out = res == -1 ? -errno : 0;
    log_op("MKDIR", path, out);
    return out;
}

/*
 * fs_rmdir
 * -------
 * Удаляет пустую директорию, соответствующую `path`.
 */
int fs_rmdir(const char *path) {
    char full[PATH_MAX_LEN];
    if (ensure_fullpath(full, path) != 0) { log_op("RMDIR", path, -EACCES); return -EACCES; }
    int res = rmdir(full);
    int out = res == -1 ? -errno : 0;
    log_op("RMDIR", path, out);
    return out;
}

/*
 * fs_release
 * ----------
 * Закрывает файл — обратный вызов для open/create. Если в fi->fh хранится
 * файловый дескриптор, он закрывается.
 */
int fs_release(const char *path, struct fuse_file_info *fi) {
    (void) path;
    if (fi && fi->fh) {
        close((int)fi->fh);
        fi->fh = 0;
    }
    log_op("RELEASE", path, 0);
    return 0;
}

/*
 * fs_utimens
 * ----------
 * Устанавливает временные метки файла (atime, mtime). Используем utimensat
 * для установки времён по полному пути. Возвращает 0 при успехе или
 * отрицательный errno.
 */
int fs_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi) {
    (void) fi;
    char full[PATH_MAX_LEN];
    if (ensure_fullpath(full, path) != 0) { log_op("UTIMENS", path, -EACCES); return -EACCES; }
    int res = utimensat(AT_FDCWD, full, tv, 0);
    int out = res == -1 ? -errno : 0;
    log_op("UTIMENS", path, out);
    return out;
}

/*
 * fs_access
 * ---------
 * Проверяет права доступа к файлу (r,w,x). Использует access() на полном пути.
 */
int fs_access(const char *path, int mask) {
    char full[PATH_MAX_LEN];
    if (ensure_fullpath(full, path) != 0) { log_op("ACCESS", path, -EACCES); return -EACCES; }
    int res = access(full, mask);
    int out = res == -1 ? -errno : 0;
    log_op("ACCESS", path, out);
    return out;
}

/*
 * fs_flush
 * -------
 * Flush вызывается для синхронизации данных. Для passthrough FS можно
 * просто вернуть 0 (no-op) — данные синхронизируются реальным файловым API.
 */
int fs_flush(const char *path, struct fuse_file_info *fi) {
    (void) path; (void) fi;
    log_op("FLUSH", path, 0);
    return 0;
}
