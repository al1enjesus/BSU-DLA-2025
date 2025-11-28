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
#include <pthread.h>

/* Глобальная переменная для хранения базовой директории */
static char *base_path = NULL;

/* Структура для хранения статистики */
static struct {
    int reads;
    int writes;
    int opens;
    int getattrs;
    int readdirs;
    int creates;
    int unlinks;
    int mkdirs;
    int rmdirs;
    size_t bytes_read;
    size_t bytes_written;
} stats;

/* Мьютекс для потокобезопасности */
static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Вспомогательная функция: получить текущий timestamp для логов */
static void log_operation(const char *op, const char *path, int result) {
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    fprintf(stderr, "[%s] %s: %s (result: %d)\n", timestamp, op, path, result);
}

/* Макрос для обновления статистики */
#define STATS_INC(field) do { \
    pthread_mutex_lock(&stats_mutex); \
    stats.field++; \
    pthread_mutex_unlock(&stats_mutex); \
} while(0)

#define STATS_ADD(field, value) do { \
    pthread_mutex_lock(&stats_mutex); \
    stats.field += (value); \
    pthread_mutex_unlock(&stats_mutex); \
} while(0)

/* Построить полный путь: base_path + relative_path */
static void get_full_path(char *fullpath, const char *path) {
    strcpy(fullpath, base_path);
    if (strcmp(path, "/") != 0) {
        strcat(fullpath, path);
    }
}

/* Функция для получения строки статистики */
static char* get_stats_string(void) {
    pthread_mutex_lock(&stats_mutex);
    
    char *stats_str;
    int len = asprintf(&stats_str,
        "reads: %d\n"
        "writes: %d\n"
        "opens: %d\n"
        "getattrs: %d\n"
        "readdirs: %d\n"
        "creates: %d\n"
        "unlinks: %d\n"
        "mkdirs: %d\n"
        "rmdirs: %d\n"
        "bytes_read: %zu\n"
        "bytes_written: %zu\n",
        stats.reads, stats.writes, stats.opens, stats.getattrs, stats.readdirs,
        stats.creates, stats.unlinks, stats.mkdirs, stats.rmdirs,
        stats.bytes_read, stats.bytes_written);
    
    pthread_mutex_unlock(&stats_mutex);
    
    if (len == -1) return NULL;
    return stats_str;
}

/*
 * getattr - получить атрибуты файла (аналог stat)
 */
static int monitoring_getattr(const char *path, struct stat *stbuf,
                             struct fuse_file_info *fi) {
    (void) fi;
    
    STATS_INC(getattrs);
    
    /* Специальная обработка для виртуального файла статистики */
    if (strcmp(path, "/.stats") == 0) {
        memset(stbuf, 0, sizeof(struct stat));
        stbuf->st_mode = S_IFREG | 0444;  // read-only
        stbuf->st_nlink = 1;
        
        char *stats_str = get_stats_string();
        if (stats_str) {
            stbuf->st_size = strlen(stats_str);
            free(stats_str);
        } else {
            stbuf->st_size = 0;
        }
        
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

    STATS_INC(readdirs);
    
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
    
    /* Добавляем виртуальный файл .stats в корневую директорию */
    if (strcmp(path, "/") == 0) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_mode = S_IFREG | 0444;
        st.st_nlink = 1;
        st.st_size = 0;  // Размер будет вычисляться динамически
        
        filler(buf, ".stats", &st, 0, 0);
        count++;
    }

    log_operation("READDIR", path, count);
    return 0;
}

/*
 * open - открыть файл
 */
static int monitoring_open(const char *path, struct fuse_file_info *fi) {
    STATS_INC(opens);
    
    /* Специальная обработка для виртуального файла статистики */
    if (strcmp(path, "/.stats") == 0) {
        /* Проверяем, что файл открывается только для чтения */
        if ((fi->flags & O_ACCMODE) != O_RDONLY) {
            log_operation("OPEN", path, -EACCES);
            return -EACCES;
        }
        log_operation("OPEN", path, 0);
        return 0;
    }
    
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
static int monitoring_read(const char *path, char *buf, size_t size, off_t offset,
                          struct fuse_file_info *fi) {
    (void) fi;
    
    STATS_INC(reads);
    
    /* Специальная обработка для виртуального файла статистики */
    if (strcmp(path, "/.stats") == 0) {
        char *stats_str = get_stats_string();
        if (!stats_str) {
            log_operation("READ", path, -ENOMEM);
            return -ENOMEM;
        }
        
        size_t stats_len = strlen(stats_str);
        
        /* Проверяем границы */
        if (offset >= stats_len) {
            free(stats_str);
            return 0;
        }
        
        if (offset + size > stats_len) {
            size = stats_len - offset;
        }
        
        /* Копируем данные статистики */
        memcpy(buf, stats_str + offset, size);
        
        STATS_ADD(bytes_read, size);
        
        time_t now = time(NULL);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(stderr, "[%s] READ: %s (%zu bytes at offset %ld, result: %zu)\n",
                timestamp, path, size, offset, size);
        
        free(stats_str);
        return size;
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
    else
        STATS_ADD(bytes_read, res);

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
    
    STATS_INC(writes);
    
    /* Запрещаем запись в виртуальный файл статистики */
    if (strcmp(path, "/.stats") == 0) {
        log_operation("WRITE", path, -EACCES);
        return -EACCES;
    }
    
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
    else
        STATS_ADD(bytes_written, res);

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
    STATS_INC(creates);
    
    /* Запрещаем создание файла .stats */
    if (strcmp(path, "/.stats") == 0) {
        log_operation("CREATE", path, -EACCES);
        return -EACCES;
    }
    
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
static int monitoring_unlink(const char *path) {
    STATS_INC(unlinks);
    
    /* Запрещаем удаление виртуального файла статистики */
    if (strcmp(path, "/.stats") == 0) {
        log_operation("UNLINK", path, -EACCES);
        return -EACCES;
    }
    
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
static int monitoring_mkdir(const char *path, mode_t mode) {
    STATS_INC(mkdirs);
    
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
static int monitoring_rmdir(const char *path) {
    STATS_INC(rmdirs);
    
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
    /* Инициализация статистики */
    memset(&stats, 0, sizeof(stats));
    
    /* Проверка аргументов */
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mount_point> [fuse_options]\n", argv[0]);
        fprintf(stderr, "Example: %s /tmp/source /mnt/fuse -f\n", argv[0]);
        fprintf(stderr, "\nOptions:\n");
        fprintf(stderr, "  -f  foreground mode (see logs)\n");
        fprintf(stderr, "  -d  debug mode (detailed FUSE output)\n");
        fprintf(stderr, "  -s  single-threaded mode\n");
        fprintf(stderr, "\nVirtual file: /.stats - shows filesystem statistics\n");
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

    fprintf(stderr, "=== FUSE Monitoring Filesystem ===\n");
    fprintf(stderr, "Source: %s\n", base_path);
    fprintf(stderr, "Mount:  %s\n", argv[2]);
    fprintf(stderr, "Virtual file: /.stats - real-time statistics\n");
    fprintf(stderr, "To unmount: fusermount -u %s\n\n", argv[2]);

    /* Запустить FUSE */
    int fuse_argc = argc - 1;
    char **fuse_argv = malloc(sizeof(char*) * (fuse_argc + 1));
    fuse_argv[0] = argv[0];
    for (int i = 2; i < argc; i++) {
        fuse_argv[i-1] = argv[i];
    }
    fuse_argv[fuse_argc] = NULL;

    int ret = fuse_main(fuse_argc, fuse_argv, &monitoring_oper, NULL);

    free(fuse_argv);
    free(base_path);

    return ret;
}
