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
#include <pthread.h>
#include <limits.h>

// Статистика операций
typedef struct {
    unsigned long reads;
    unsigned long writes;
    unsigned long opens;
    unsigned long getattrs;
    unsigned long readdirs;
    unsigned long creates;
    unsigned long unlinks;
    unsigned long mkdirs;
    unsigned long rmdirs;
    unsigned long long bytes_read;
    unsigned long long bytes_written;
    pthread_mutex_t lock;
} stats_t;

static char *base_path = NULL;
static stats_t stats = {0};
static const char *STATS_FILE = "/.stats";

/**
 * Инициализация статистики
 */
static void init_stats() {
    pthread_mutex_init(&stats.lock, NULL);
}

/**
 * Обновление статистики
 */
static void update_stats(const char *op, ssize_t bytes) {
    pthread_mutex_lock(&stats.lock);
    
    if (strcmp(op, "read") == 0) {
        stats.reads++;
        if (bytes > 0) stats.bytes_read += bytes;
    } else if (strcmp(op, "write") == 0) {
        stats.writes++;
        if (bytes > 0) stats.bytes_written += bytes;
    } else if (strcmp(op, "open") == 0) {
        stats.opens++;
    } else if (strcmp(op, "getattr") == 0) {
        stats.getattrs++;
    } else if (strcmp(op, "readdir") == 0) {
        stats.readdirs++;
    } else if (strcmp(op, "create") == 0) {
        stats.creates++;
    } else if (strcmp(op, "unlink") == 0) {
        stats.unlinks++;
    } else if (strcmp(op, "mkdir") == 0) {
        stats.mkdirs++;
    } else if (strcmp(op, "rmdir") == 0) {
        stats.rmdirs++;
    }
    
    pthread_mutex_unlock(&stats.lock);
}

/**
 * Генерация содержимого файла .stats
 */
static void generate_stats_content(char *buf, size_t size) {
    pthread_mutex_lock(&stats.lock);
    
    snprintf(buf, size,
             "reads: %lu\n"
             "writes: %lu\n"
             "opens: %lu\n"
             "getattrs: %lu\n"
             "readdirs: %lu\n"
             "creates: %lu\n"
             "unlinks: %lu\n"
             "mkdirs: %lu\n"
             "rmdirs: %lu\n"
             "bytes_read: %llu\n"
             "bytes_written: %llu\n",
             stats.reads,
             stats.writes,
             stats.opens,
             stats.getattrs,
             stats.readdirs,
             stats.creates,
             stats.unlinks,
             stats.mkdirs,
             stats.rmdirs,
             stats.bytes_read,
             stats.bytes_written);
    
    pthread_mutex_unlock(&stats.lock);
}

/**
 * Построение полного пути
 */
static void get_full_path(char *full_path, const char *path) {
    strcpy(full_path, base_path);
    strncat(full_path, path, PATH_MAX - strlen(base_path) - 1);
}

/**
 * Проверка, является ли путь виртуальным файлом статистики
 */
static int is_stats_file(const char *path) {
    return strcmp(path, STATS_FILE) == 0;
}

/**
 * Получение атрибутов файла
 */
static int mon_getattr(const char *path, struct stat *stbuf,
                       struct fuse_file_info *fi) {
    (void) fi;
    
    update_stats("getattr", 0);
    
    if (is_stats_file(path)) {
        memset(stbuf, 0, sizeof(struct stat));
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        
        char buf[1024];
        generate_stats_content(buf, sizeof(buf));
        stbuf->st_size = strlen(buf);
        
        return 0;
    }
    
    char full_path[PATH_MAX];
    get_full_path(full_path, path);
    
    int res = lstat(full_path, stbuf);
    if (res == -1)
        return -errno;
    
    return 0;
}

/**
 * Чтение директории
 */
static int mon_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;
    
    update_stats("readdir", 0);
    
    char full_path[PATH_MAX];
    get_full_path(full_path, path);
    
    DIR *dp = opendir(full_path);
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
    
    // Добавляем виртуальный файл .stats в корневой директории
    if (strcmp(path, "/") == 0) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_mode = S_IFREG | 0444;
        filler(buf, ".stats", &st, 0, 0);
    }
    
    closedir(dp);
    return 0;
}

/**
 * Открытие файла
 */
static int mon_open(const char *path, struct fuse_file_info *fi) {
    update_stats("open", 0);
    
    if (is_stats_file(path)) {
        if ((fi->flags & O_ACCMODE) != O_RDONLY)
            return -EACCES;
        return 0;
    }
    
    char full_path[PATH_MAX];
    get_full_path(full_path, path);
    
    int fd = open(full_path, fi->flags);
    if (fd == -1)
        return -errno;
    
    close(fd);
    return 0;
}

/**
 * Чтение файла
 */
static int mon_read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
    (void) fi;
    
    if (is_stats_file(path)) {
        char stats_buf[1024];
        generate_stats_content(stats_buf, sizeof(stats_buf));
        
        size_t len = strlen(stats_buf);
        if (offset >= (off_t)len)
            return 0;
        
        if (offset + size > len)
            size = len - offset;
        
        memcpy(buf, stats_buf + offset, size);
        
        update_stats("read", size);
        return size;
    }
    
    char full_path[PATH_MAX];
    get_full_path(full_path, path);
    
    int fd = open(full_path, O_RDONLY);
    if (fd == -1)
        return -errno;
    
    int res = pread(fd, buf, size, offset);
    if (res == -1)
        res = -errno;
    
    close(fd);
    
    if (res > 0)
        update_stats("read", res);
    
    return res;
}

/**
 * Запись в файл
 */
static int mon_write(const char *path, const char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi) {
    (void) fi;
    
    if (is_stats_file(path))
        return -EACCES;
    
    char full_path[PATH_MAX];
    get_full_path(full_path, path);
    
    int fd = open(full_path, O_WRONLY);
    if (fd == -1)
        return -errno;
    
    int res = pwrite(fd, buf, size, offset);
    if (res == -1)
        res = -errno;
    
    close(fd);
    
    if (res > 0)
        update_stats("write", res);
    
    return res;
}

/**
 * Создание файла
 */
static int mon_create(const char *path, mode_t mode,
                     struct fuse_file_info *fi) {
    if (is_stats_file(path))
        return -EACCES;
    
    char full_path[PATH_MAX];
    get_full_path(full_path, path);
    
    int fd = open(full_path, fi->flags, mode);
    if (fd == -1)
        return -errno;
    
    close(fd);
    update_stats("create", 0);
    return 0;
}

/**
 * Удаление файла
 */
static int mon_unlink(const char *path) {
    if (is_stats_file(path))
        return -EACCES;
    
    char full_path[PATH_MAX];
    get_full_path(full_path, path);
    
    int res = unlink(full_path);
    if (res == -1)
        return -errno;
    
    update_stats("unlink", 0);
    return 0;
}

/**
 * Создание директории
 */
static int mon_mkdir(const char *path, mode_t mode) {
    char full_path[PATH_MAX];
    get_full_path(full_path, path);
    
    int res = mkdir(full_path, mode);
    if (res == -1)
        return -errno;
    
    update_stats("mkdir", 0);
    return 0;
}

/**
 * Удаление директории
 */
static int mon_rmdir(const char *path) {
    char full_path[PATH_MAX];
    get_full_path(full_path, path);
    
    int res = rmdir(full_path);
    if (res == -1)
        return -errno;
    
    update_stats("rmdir", 0);
    return 0;
}

/**
 * Таблица операций FUSE
 */
static struct fuse_operations mon_oper = {
    .getattr = mon_getattr,
    .readdir = mon_readdir,
    .open = mon_open,
    .read = mon_read,
    .write = mon_write,
    .create = mon_create,
    .unlink = mon_unlink,
    .mkdir = mon_mkdir,
    .rmdir = mon_rmdir,
};

/**
 * Главная функция
 */
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mount_point>\n", argv[0]);
        return 1;
    }
    
    base_path = realpath(argv[1], NULL);
    if (base_path == NULL) {
        perror("realpath");
        return 1;
    }
    
    init_stats();
    
    // Удаляем первый аргумент для FUSE
    for (int i = 1; i < argc - 1; i++) {
        argv[i] = argv[i + 1];
    }
    argc--;
    
    fprintf(stderr, "Monitoring %s at %s\n", base_path, argv[1]);
    fprintf(stderr, "Statistics available at %s%s\n", argv[1], STATS_FILE);
    
    int ret = fuse_main(argc, argv, &mon_oper, NULL);
    
    pthread_mutex_destroy(&stats.lock);
    free(base_path);
    return ret;
}
