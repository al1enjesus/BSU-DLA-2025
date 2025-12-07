#ifndef FUSE_FS_H
#define FUSE_FS_H

#define _GNU_SOURCE
#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <dirent.h>
#include <limits.h>

// Режимы работы
enum fs_mode {
    MODE_PASSTHROUGH,
    MODE_TAR_READONLY
};

struct fuse_fs_config {
    char source_path[PATH_MAX]; // директория ИЛИ .tar файл
    enum fs_mode mode;
    int stats_enabled;          // всегда включено для passthrough
};

extern struct fuse_fs_config config;
extern pthread_mutex_t stats_mutex;

// Статистика
extern long stats_reads;
extern long stats_writes;
extern long stats_opens;
extern long long stats_bytes_read;
extern long long stats_bytes_written;

// Утилиты
void log_op(const char *op, const char *path, int result);
int ends_with(const char *str, const char *suffix);

// Tar
typedef struct {
    char *data;        // mmap или malloc
    size_t size;
    // виртуальная файловая структура: путь -> смещение+размер
    struct tar_entry *entries;
    size_t entry_count;
} tar_archive_t;

typedef struct {
    char path[256];
    size_t offset;
    size_t size;
    mode_t mode;
    time_t mtime;
} tar_entry_t;

extern tar_archive_t g_tar;

int tar_load(const char *tar_path);
void tar_free(tar_archive_t *tar);
tar_entry_t *tar_find(const tar_archive_t *tar, const char *path);
int tar_list_root(const tar_archive_t *tar, void *buf, fuse_fill_dir_t filler);

// Статистика (.stats)
int is_stats_file(const char *path);
int stats_getattr(const char *path, struct stat *stbuf);
int stats_readdir(const char *path, void *buf, fuse_fill_dir_t filler);
int stats_read(const char *path, char *buf, size_t size, off_t offset);

// Операции
extern struct fuse_operations passthrough_ops;
extern struct fuse_operations tar_ops;

#endif