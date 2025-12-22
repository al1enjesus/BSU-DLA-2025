/*
 * Archive FUSE filesystem - Задание B (Вариант 1)
 *
 * Read-only FS для просмотра содержимого tar архива.
 * Использование: ./archive <tar_file> <mount_point>
 *
 * Требует libarchive-dev
 */

#define FUSE_USE_VERSION 31

#include <stddef.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <archive.h>
#include <archive_entry.h>
#include <time.h>
#include <sys/stat.h>

/* Структура для хранения файла из архива */
typedef struct archive_file {
    char *path;
    char *data;
    size_t size;
    struct stat st;
    struct archive_file *next;
} archive_file_t;

/* Глобальная переменная для списка файлов */
static archive_file_t *files = NULL;
static char *tar_path = NULL;

/* Вспомогательная функция: получить текущий timestamp для логов */
static void log_operation(const char *op, const char *path, int result) {
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    fprintf(stderr, "[%s] %s: %s (result: %d)\n", timestamp, op, path, result);
}

/* Найти файл по пути */
static archive_file_t *find_file(const char *path) {
    archive_file_t *f = files;
    while (f) {
        if (strcmp(f->path, path) == 0) {
            return f;
        }
        f = f->next;
    }
    return NULL;
}

/* Загрузить архив в память */
static int load_archive(const char *tarfile) {
    struct archive *a = archive_read_new();
    archive_read_support_format_tar(a);
    archive_read_support_filter_gzip(a);

    if (archive_read_open_filename(a, tarfile, 10240) != ARCHIVE_OK) {
        fprintf(stderr, "Failed to open archive: %s\n", archive_error_string(a));
        return -1;
    }

    struct archive_entry *entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char *path = archive_entry_pathname(entry);
        size_t size = archive_entry_size(entry);

        archive_file_t *f = malloc(sizeof(archive_file_t));
        f->path = strdup(path);
        f->size = size;
        f->data = NULL;
        f->next = files;
        files = f;

        // Заполнить stat
        const struct stat *st = archive_entry_stat(entry);
        memcpy(&f->st, st, sizeof(struct stat));

        if (size > 0) {
            f->data = malloc(size);
            if (!f->data) {
                fprintf(stderr, "Failed to allocate memory for file data\n");
                // Continue, but data is NULL, will cause issues later
            } else {
                archive_read_data(a, f->data, size);
            }
        }
    }

    archive_read_free(a);
    return 0;
}

/*
 * getattr - получить атрибуты файла
 */
static int archive_getattr(const char *path, struct stat *stbuf,
                           struct fuse_file_info *fi) {
    (void) fi;

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        log_operation("GETATTR", path, 0);
        return 0;
    }

    archive_file_t *f = find_file(path + 1); // skip leading /
    if (!f) {
        log_operation("GETATTR", path, -ENOENT);
        return -ENOENT;
    }

    memcpy(stbuf, &f->st, sizeof(struct stat));
    log_operation("GETATTR", path, 0);
    return 0;
}

/*
 * readdir - прочитать содержимое директории
 */
static int archive_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                           off_t offset, struct fuse_file_info *fi,
                           enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    if (strcmp(path, "/") != 0) {
        log_operation("READDIR", path, -ENOENT);
        return -ENOENT;
    }

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    archive_file_t *f = files;
    while (f) {
        filler(buf, f->path, NULL, 0, 0);
        f = f->next;
    }

    log_operation("READDIR", path, 0);
    return 0;
}

/*
 * open - открыть файл (только для чтения)
 */
static int archive_open(const char *path, struct fuse_file_info *fi) {
    if ((fi->flags & O_ACCMODE) != O_RDONLY) {
        log_operation("OPEN", path, -EACCES);
        return -EACCES;
    }

    archive_file_t *f = find_file(path + 1);
    if (!f) {
        log_operation("OPEN", path, -ENOENT);
        return -ENOENT;
    }

    log_operation("OPEN", path, 0);
    return 0;
}

/*
 * read - прочитать данные из файла
 */
static int archive_read(const char *path, char *buf, size_t size, off_t offset,
                        struct fuse_file_info *fi) {
    (void) fi;

    archive_file_t *f = find_file(path + 1);
    if (!f) {
        log_operation("READ", path, -ENOENT);
        return -ENOENT;
    }

    if (offset >= f->size) {
        return 0;
    }

    if (!f->data) {
        log_operation("READ", path, -EIO);
        return -EIO;
    }

    size_t to_read = size;
    if (offset + size > f->size) {
        to_read = f->size - offset;
    }

    memcpy(buf, f->data + offset, to_read);

    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(stderr, "[%s] READ: %s (%zu bytes at offset %ld, result: %d)\n",
            timestamp, path, size, offset, (int)to_read);

    return to_read;
}

/* Структура с указателями на операции */
static struct fuse_operations archive_oper = {
    .getattr    = archive_getattr,
    .readdir    = archive_readdir,
    .open       = archive_open,
    .read       = archive_read,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <tar_file> <mount_point> [fuse_options]\n", argv[0]);
        return 1;
    }

    tar_path = argv[1];

    if (load_archive(tar_path) != 0) {
        return 1;
    }

    fprintf(stderr, "Archive loaded: %s\n", tar_path);

    /* Запустить FUSE */
    int fuse_argc = argc - 1;
    char **fuse_argv = malloc(sizeof(char*) * fuse_argc);
    fuse_argv[0] = argv[0];
    for (int i = 2; i < argc; i++) {
        fuse_argv[i-1] = argv[i];
    }

    int ret = fuse_main(fuse_argc, fuse_argv, &archive_oper, NULL);

    free(fuse_argv);

    // Очистка памяти
    archive_file_t *f = files;
    while (f) {
        archive_file_t *next = f->next;
        free(f->path);
        free(f->data);
        free(f);
        f = next;
    }

    return ret;
}