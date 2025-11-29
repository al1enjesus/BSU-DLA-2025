#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/time.h>

// --- Глобальные переменные ---

static char *source_dir = NULL;
static const char *STATS_FILE = "/.stats";

// Структура статистики
struct fs_stats {
    unsigned long reads;
    unsigned long writes;
    unsigned long opens;
    unsigned long bytes_read;
    unsigned long bytes_written;
    pthread_mutex_t lock; // Мьютекс для защиты данных в многопоточной среде
};

// Инициализация статистики с мьютексом
struct fs_stats stats = {
    .reads = 0,
    .writes = 0,
    .opens = 0,
    .bytes_read = 0,
    .bytes_written = 0,
    .lock = PTHREAD_MUTEX_INITIALIZER
};

// --- Вспомогательные функции ---

// Получение полного пути к реальному файлу
static void get_full_path(char *fpath, const char *path) {
    strcpy(fpath, source_dir);
    // Убираем двойной слеш, если source_dir заканчивается на /
    if (fpath[strlen(fpath) - 1] == '/') {
        fpath[strlen(fpath) - 1] = '\0';
    }
    strcat(fpath, path);
}

// Обновление статистики (потокобезопасно)
static void update_stats(int type, size_t bytes) {
    pthread_mutex_lock(&stats.lock);
    switch (type) {
        case 0: stats.opens++; break;
        case 1: 
            stats.reads++; 
            stats.bytes_read += bytes; 
            break;
        case 2: 
            stats.writes++; 
            stats.bytes_written += bytes; 
            break;
    }
    pthread_mutex_unlock(&stats.lock);
}

// --- FUSE Operations ---

// Получение атрибутов файла (stat)
// ИСПРАВЛЕНО: добавлена сигнатура для FUSE 3 (struct fuse_file_info *fi)
static int mon_getattr(const char *path, struct stat *st, struct fuse_file_info *fi) {
    (void) fi; // Не используем, но обязан быть в сигнатуре
    memset(st, 0, sizeof(struct stat));

    // Виртуальный файл статистики
    if (strcmp(path, STATS_FILE) == 0) {
        st->st_mode = S_IFREG | 0444; // Read-only
        st->st_nlink = 1;
        st->st_size = 512; // Примерный размер, чтобы cat не думал, что файл пустой
        return 0;
    }

    char fpath[1024];
    get_full_path(fpath, path);

    if (lstat(fpath, st) == -1)
        return -errno;

    return 0;
}

// Чтение директории (ls)
static int mon_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    char fpath[1024];
    get_full_path(fpath, path);

    DIR *dp = opendir(fpath);
    if (dp == NULL)
        return -errno;

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0, 0))
            break;
    }

    // Если читаем корень, добавляем виртуальный файл .stats
    if (strcmp(path, "/") == 0) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_mode = S_IFREG | 0444;
        // ".stats" + 1 убирает начальный слеш для отображения имени
        filler(buf, STATS_FILE + 1, &st, 0, 0); 
    }

    closedir(dp);
    return 0;
}

// Открытие файла
static int mon_open(const char *path, struct fuse_file_info *fi) {
    update_stats(0, 0); // Open count

    // Если открывают .stats
    if (strcmp(path, STATS_FILE) == 0) {
        if ((fi->flags & O_ACCMODE) != O_RDONLY)
            return -EACCES; // Запрещаем запись
        return 0;
    }

    char fpath[1024];
    get_full_path(fpath, path);

    int fd = open(fpath, fi->flags);
    if (fd == -1)
        return -errno;

    fi->fh = fd;
    return 0;
}
// Чтение из файла
static int mon_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi) {
    
    // Генерация содержимого .stats
    if (strcmp(path, STATS_FILE) == 0) {
        char report[1024];
        
        pthread_mutex_lock(&stats.lock);
        int len = snprintf(report, sizeof(report),
            "--- FUSE Monitor Statistics ---\n"
            "Opens:         %lu\n"
            "Reads:         %lu\n"
            "Writes:        %lu\n"
            "Bytes Read:    %lu\n"
            "Bytes Written: %lu\n",
            stats.opens, stats.reads, stats.writes, 
            stats.bytes_read, stats.bytes_written);
        pthread_mutex_unlock(&stats.lock);

        if (offset >= len) return 0;
        if (offset + size > len) size = len - offset;
        
        memcpy(buf, report + offset, size);
        return size;
    }

    // Обычное чтение
    int res = pread(fi->fh, buf, size, offset);
    if (res == -1) return -errno;

    update_stats(1, res); // Read stats
    return res;
}

// Запись в файл
static int mon_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi) {
    
    if (strcmp(path, STATS_FILE) == 0)
        return -EACCES;

    int res = pwrite(fi->fh, buf, size, offset);
    if (res == -1) return -errno;

    update_stats(2, res); // Write stats
    return res;
}

// Создание файла
static int mon_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char fpath[1024]; get_full_path(fpath, path);
    int res = open(fpath, fi->flags, mode);
    if (res == -1) return -errno;
    fi->fh = res;
    update_stats(0, 0); // Считаем создание как Open
    return 0;
}

// Удаление файла
static int mon_unlink(const char *path) {
    char fpath[1024]; get_full_path(fpath, path);
    int res = unlink(fpath);
    return (res == -1) ? -errno : 0;
}

// Создание директории
static int mon_mkdir(const char *path, mode_t mode) {
    char fpath[1024]; get_full_path(fpath, path);
    int res = mkdir(fpath, mode);
    return (res == -1) ? -errno : 0;
}

// Удаление директории
static int mon_rmdir(const char *path) {
    char fpath[1024]; get_full_path(fpath, path);
    int res = rmdir(fpath);
    return (res == -1) ? -errno : 0;
}

// Регистрация операций
static struct fuse_operations operations = {
    .getattr = mon_getattr,
    .readdir = mon_readdir,
    .open    = mon_open,
    .read    = mon_read,
    .write   = mon_write,
    .create  = mon_create,
    .unlink  = mon_unlink,
    .mkdir   = mon_mkdir,
    .rmdir   = mon_rmdir,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mount_point>\n", argv[0]);
        return 1;
    }

    source_dir = realpath(argv[1], NULL);
    if (!source_dir) {
        perror("Source directory not found");
        return 1;
    }

    // Подготовка аргументов. Используем argv[0] и mount_point (argv[2])
    // Добавляем флаг -f, чтобы запускать не в фоне (удобнее для лабы)
    char *fuse_argv[] = { argv[0], argv[2], "-f", NULL };
    
    return fuse_main(3, fuse_argv, &operations, NULL);
}
