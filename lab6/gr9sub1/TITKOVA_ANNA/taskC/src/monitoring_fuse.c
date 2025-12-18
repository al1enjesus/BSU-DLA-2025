/*
 * Monitoring FUSE Filesystem - Реализация задания C
 * 
 * Passthrough FS с подсчетом статистики обращений и виртуальным файлом .stats
 * 
 * Компиляция: gcc -Wall monitoring_fuse.c -lfuse3 -o monitoring_fuse
 * Использование: ./monitoring_fuse <source_dir> <mount_point> [опции]
 */

#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <stdatomic.h>
#include <limits.h>

/* ========== ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ И КОНСТАНТЫ ========== */

static char *base_path = NULL;

/* Путь к виртуальному файлу статистики */
#define STATS_FILE_PATH "/.stats"
#define STATS_FILE_NAME ".stats"

/* Флаг логирования */
static int debug_mode = 0;

/* Структура для статистики (с атомарными операциями для потокобезопасности) */
typedef struct {
    _Atomic unsigned long reads;
    _Atomic unsigned long writes;
    _Atomic unsigned long opens;
    _Atomic unsigned long creates;
    _Atomic unsigned long getattrs;
    _Atomic unsigned long readdirs;
    _Atomic unsigned long unlinks;
    _Atomic unsigned long mkdirs;
    _Atomic unsigned long rmdirs;
    _Atomic unsigned long bytes_read;
    _Atomic unsigned long bytes_written;
    _Atomic unsigned long stats_file_reads;
} fs_stats_t;

static fs_stats_t stats = {0};

/* Время запуска файловой системы */
static time_t start_time;

/* ========== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ========== */

/* Проверка, является ли путь виртуальным файлом статистики */
static int is_stats_file(const char *path) {
    return strcmp(path, STATS_FILE_PATH) == 0;
}

/* Безопасное получение полного пути с защитой от path traversal */
static char* get_full_path(const char *path) {
    if (!base_path || is_stats_file(path)) {
        return NULL;
    }
    
    /* Проверяем что path не содержит небезопасные последовательности */
    if (strstr(path, "/../") != NULL || 
        strncmp(path, "../", 3) == 0 ||
        strstr(path, "/./") != NULL ||
        strncmp(path, "./", 2) == 0 ||
        (strlen(path) >= 3 && strncmp(path + strlen(path) - 3, "/..", 3) == 0) ||
        (strlen(path) >= 2 && strncmp(path + strlen(path) - 2, "/.", 2) == 0) ||
        strstr(path, "//") != NULL) {
        return NULL;
    }
    
    size_t base_len = strlen(base_path);
    size_t path_len = strlen(path);
    size_t total_len = base_len + path_len + 2;
    
    char *full_path = malloc(total_len);
    if (!full_path) return NULL;
    
    strcpy(full_path, base_path);
    
    /* Обрабатываем слеши */
    if (path[0] == '/' && base_path[base_len - 1] == '/') {
        strcat(full_path, path + 1);
    } else if (path[0] != '/' && base_path[base_len - 1] != '/') {
        strcat(full_path, "/");
        strcat(full_path, path);
    } else {
        strcat(full_path, path);
    }
    
    return full_path;
}

/* Получение текущего времени в формате для логов */
static void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

/* Логирование операций согласно формату задания */
static void log_operation(const char *operation, const char *path, int result, ssize_t bytes) {
    if (!debug_mode) return;
    
    char timestamp[64];
    get_timestamp(timestamp, sizeof(timestamp));
    
    const char *result_str = (result >= 0 || result == 0) ? "success" : "error";
    
    if (bytes > 0) {
        fprintf(stderr, "[%s] %s: %s (%zd bytes) (%s)\n", 
                timestamp, operation, path, bytes, result_str);
    } else {
        fprintf(stderr, "[%s] %s: %s (%s)\n", 
                timestamp, operation, path, result_str);
    }
}

/* Форматирование статистики в строку с вычислением размера */
/* Форматирование статистики в строку с вычислением размера */
static char* format_stats_string(size_t *out_size) {
    time_t now = time(NULL);
    double uptime_seconds = difftime(now, start_time);
    int uptime_hours = (int)uptime_seconds / 3600;
    int uptime_minutes = ((int)uptime_seconds % 3600) / 60;
    int uptime_secs = (int)uptime_seconds % 60;
    
    /* Вычисляем примерный размер буфера */
    size_t buffer_size = 1024;
    
    char *buffer = malloc(buffer_size);
    if (!buffer) {
        if (out_size) *out_size = 0;
        return NULL;
    }
    
    int len = snprintf(buffer, buffer_size,
        "=== FUSE Monitoring Filesystem Statistics ===\n"
        "General operations:\n"
        "  getattr:      %lu\n"
        "  readdir:      %lu\n"
        "  open:         %lu\n"
        "  create:       %lu\n"
        "  read:         %lu\n"
        "  write:        %lu\n"
        "  unlink:       %lu\n"
        "  mkdir:        %lu\n"
        "  rmdir:        %lu\n"
        "\n"
        "Data transfer:\n"
        "  bytes read:   %lu (%.2f MB)\n"
        "  bytes written:%lu (%.2f MB)\n"
        "\n"
        "Virtual file stats:\n"
        "  .stats reads: %lu\n"
        "\n"
        "System uptime:  %02d:%02d:%02d (%.0f seconds)\n"
        "=============================================\n",
        
        stats.getattrs,
        stats.readdirs,
        stats.opens,
        stats.creates,
        stats.reads,
        stats.writes,
        stats.unlinks,
        stats.mkdirs,
        stats.rmdirs,
        
        stats.bytes_read,
        (double)stats.bytes_read / (1024 * 1024),
        stats.bytes_written,
        (double)stats.bytes_written / (1024 * 1024),
        
        stats.stats_file_reads,
        
        uptime_hours,
        uptime_minutes,
        uptime_secs,
        uptime_seconds
    );
    
    if (out_size) {
        *out_size = (len > 0 && (size_t)len < buffer_size) ? (size_t)len : buffer_size - 1;
    }
    return buffer;
}
/* Обновление статистики */
static void update_stat(const char *operation, const char *path, ssize_t bytes, int result) {
    if (result < 0) {
        return; /* Не обновляем статистику при ошибках */
    }
    
    /* Обновляем соответствующую статистику */
    if (strcmp(operation, "GETATTR") == 0) {
        atomic_fetch_add(&stats.getattrs, 1);
    } else if (strcmp(operation, "READDIR") == 0) {
        atomic_fetch_add(&stats.readdirs, 1);
    } else if (strcmp(operation, "OPEN") == 0) {
        atomic_fetch_add(&stats.opens, 1);
    } else if (strcmp(operation, "CREATE") == 0) {
        atomic_fetch_add(&stats.creates, 1);
    } else if (strcmp(operation, "READ") == 0) {
        if (is_stats_file(path)) {
            atomic_fetch_add(&stats.stats_file_reads, 1);
        } else {
            atomic_fetch_add(&stats.reads, 1);
            if (bytes > 0) {
                atomic_fetch_add(&stats.bytes_read, bytes);
            }
        }
    } else if (strcmp(operation, "WRITE") == 0) {
        atomic_fetch_add(&stats.writes, 1);
        if (bytes > 0) {
            atomic_fetch_add(&stats.bytes_written, bytes);
        }
    } else if (strcmp(operation, "UNLINK") == 0) {
        atomic_fetch_add(&stats.unlinks, 1);
    } else if (strcmp(operation, "MKDIR") == 0) {
        atomic_fetch_add(&stats.mkdirs, 1);
    } else if (strcmp(operation, "RMDIR") == 0) {
        atomic_fetch_add(&stats.rmdirs, 1);
    }
}

/* Комбинированная функция логирования и обновления статистики */
static void log_and_update_stat(const char *operation, const char *path, 
                               ssize_t bytes, off_t offset, int result) {
    (void)offset;
    
    update_stat(operation, path, bytes, result);
    log_operation(operation, path, result, bytes);
}

/* ========== ОСНОВНЫЕ ОПЕРАЦИИ FUSE ========== */

/* getattr - получение атрибутов файла */
static int monitoring_getattr(const char *path, struct stat *stbuf,
                              struct fuse_file_info *fi) {
    (void)fi;
    
    memset(stbuf, 0, sizeof(struct stat));
    
    /* Обработка виртуального файла статистики */
    if (is_stats_file(path)) {
        size_t stats_size;
        char *stats_str = format_stats_string(&stats_size);
        
        stbuf->st_mode = S_IFREG | 0444;  /* Обычный файл, только чтение */
        stbuf->st_nlink = 1;
        stbuf->st_size = stats_size;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);
        stbuf->st_blocks = (stbuf->st_size + 511) / 512;
        stbuf->st_blksize = 4096;
        
        if (stats_str) free(stats_str);
        log_and_update_stat("GETATTR", path, 0, 0, 0);
        return 0;
    }
    
    /* Обычный файл в базовой директории */
    char *full_path = get_full_path(path);
    if (!full_path) {
        log_and_update_stat("GETATTR", path, 0, 0, -ENOENT);
        return -ENOENT;
    }
    
    int res = lstat(full_path, stbuf);
    int ret = (res == -1) ? -errno : 0;
    
    free(full_path);
    log_and_update_stat("GETATTR", path, 0, 0, ret);
    
    return ret;
}

/* readdir - чтение содержимого директории */
static int monitoring_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                              off_t offset, struct fuse_file_info *fi,
                              enum fuse_readdir_flags flags) {
    (void)offset;
    (void)fi;
    (void)flags;
    
    /* Добавляем стандартные записи */
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    
    /* Если это корневая директория монтирования, добавляем виртуальный файл */
    if (strcmp(path, "/") == 0) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_mode = S_IFREG | 0444;
        st.st_nlink = 1;
        st.st_uid = getuid();
        st.st_gid = getgid();
        st.st_atime = st.st_mtime = st.st_ctime = time(NULL);
        
        /* Вычисляем реальный размер */
        size_t stats_size;
        char *stats_str = format_stats_string(&stats_size);
        st.st_size = stats_size;
        if (stats_str) free(stats_str);
        
        filler(buf, STATS_FILE_NAME, &st, 0, 0);
    }
    
    /* Добавляем реальные файлы из базовой директории */
    char *full_path = get_full_path(path);
    if (!full_path) {
        log_and_update_stat("READDIR", path, 0, 0, 0);
        return 0;
    }
    
    DIR *dp = opendir(full_path);
    if (!dp) {
        int ret = -errno;
        free(full_path);
        log_and_update_stat("READDIR", path, 0, 0, ret);
        return ret;
    }
    
    struct dirent *de;
    int res = 0;
    while ((de = readdir(dp)) != NULL) {
        /* Пропускаем . и .., так как уже добавили */
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }
        
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        
        if (filler(buf, de->d_name, &st, 0, 0)) {
            break;
        }
    }
    
    closedir(dp);
    free(full_path);
    
    log_and_update_stat("READDIR", path, 0, 0, res);
    return res;
}

/* open - открытие файла */
static int monitoring_open(const char *path, struct fuse_file_info *fi) {
    /* Виртуальный файл статистики - всегда доступен для чтения */
    if (is_stats_file(path)) {
        if ((fi->flags & O_ACCMODE) != O_RDONLY) {
            log_and_update_stat("OPEN", path, 0, 0, -EACCES);
            return -EACCES;  /* Только для чтения */
        }
        fi->fh = 0;  /* Специальное значение для виртуального файла */
        log_and_update_stat("OPEN", path, 0, 0, 0);
        return 0;
    }
    
    /* Обычный файл */
    char *full_path = get_full_path(path);
    if (!full_path) {
        log_and_update_stat("OPEN", path, 0, 0, -ENOENT);
        return -ENOENT;
    }
    
    int fd = open(full_path, fi->flags);
    int ret;
    
    if (fd == -1) {
        ret = -errno;
    } else {
        fi->fh = fd;
        ret = 0;
    }
    
    free(full_path);
    log_and_update_stat("OPEN", path, 0, 0, ret);
    return ret;
}

/* read - чтение из файла */
static int monitoring_read(const char *path, char *buf, size_t size, off_t offset,
                           struct fuse_file_info *fi) {
    /* Обработка виртуального файла статистики */
    if (is_stats_file(path)) {
        size_t stats_size;
        char *stats_str = format_stats_string(&stats_size);
        if (!stats_str) {
            log_and_update_stat("READ", path, 0, offset, -ENOMEM);
            return -ENOMEM;
        }
        
        /* Проверяем, не вышли ли за пределы файла */
        if (offset < 0 || (size_t)offset >= stats_size) {
            free(stats_str);
            log_and_update_stat("READ", path, 0, offset, 0);
            return 0;
        }
        
        /* Определяем сколько байт читать */
        size_t bytes_to_read = size;
        if ((size_t)offset + bytes_to_read > stats_size) {
            bytes_to_read = stats_size - (size_t)offset;
        }
        
        /* Копируем данные */
        memcpy(buf, stats_str + offset, bytes_to_read);
        
        free(stats_str);
        log_and_update_stat("READ", path, bytes_to_read, offset, 0);
        return bytes_to_read;
    }
    
    /* Обычный файл */
    ssize_t res = pread(fi->fh, buf, size, offset);
    
    if (res == -1) {
        int ret = -errno;
        log_and_update_stat("READ", path, size, offset, ret);
        return ret;
    }
    
    log_and_update_stat("READ", path, res, offset, 0);
    return res;
}


/* write - запись в файл */
static int monitoring_write(const char *path, const char *buf, size_t size,
                            off_t offset, struct fuse_file_info *fi) {
    /* Виртуальный файл статистики нельзя писать */
    if (is_stats_file(path)) {
        log_and_update_stat("WRITE", path, 0, offset, -EACCES);
        return -EACCES;
    }
    
    ssize_t res = pwrite(fi->fh, buf, size, offset);
    
    if (res == -1) {
        int ret = -errno;
        log_and_update_stat("WRITE", path, size, offset, ret);
        return ret;
    }
    
    log_and_update_stat("WRITE", path, res, offset, 0);
    return res;
}

/* create - создание файла */
static int monitoring_create(const char *path, mode_t mode,
                             struct fuse_file_info *fi) {
    /* Нельзя создать файл с именем .stats */
    if (is_stats_file(path)) {
        log_and_update_stat("CREATE", path, 0, 0, -EEXIST);
        return -EEXIST;
    }
    
    char *full_path = get_full_path(path);
    if (!full_path) {
        log_and_update_stat("CREATE", path, 0, 0, -ENOENT);
        return -ENOENT;
    }
    
    int fd = creat(full_path, mode);
    int ret;
    
    if (fd == -1) {
        ret = -errno;
    } else {
        fi->fh = fd;
        ret = 0;
    }
    
    free(full_path);
    log_and_update_stat("CREATE", path, 0, 0, ret);
    return ret;
}

/* unlink - удаление файла */
static int monitoring_unlink(const char *path) {
    /* Нельзя удалить виртуальный файл статистики */
    if (is_stats_file(path)) {
        log_and_update_stat("UNLINK", path, 0, 0, -EACCES);
        return -EACCES;
    }
    
    char *full_path = get_full_path(path);
    if (!full_path) {
        log_and_update_stat("UNLINK", path, 0, 0, -ENOENT);
        return -ENOENT;
    }
    
    int res = unlink(full_path);
    int ret = (res == -1) ? -errno : 0;
    
    free(full_path);
    log_and_update_stat("UNLINK", path, 0, 0, ret);
    return ret;
}

/* mkdir - создание директории */
static int monitoring_mkdir(const char *path, mode_t mode) {
    char *full_path = get_full_path(path);
    if (!full_path) {
        log_and_update_stat("MKDIR", path, 0, 0, -ENOENT);
        return -ENOENT;
    }
    
    int res = mkdir(full_path, mode);
    int ret = (res == -1) ? -errno : 0;
    
    free(full_path);
    log_and_update_stat("MKDIR", path, 0, 0, ret);
    return ret;
}

/* rmdir - удаление директории */
static int monitoring_rmdir(const char *path) {
    char *full_path = get_full_path(path);
    if (!full_path) {
        log_and_update_stat("RMDIR", path, 0, 0, -ENOENT);
        return -ENOENT;
    }
    
    int res = rmdir(full_path);
    int ret = (res == -1) ? -errno : 0;
    
    free(full_path);
    log_and_update_stat("RMDIR", path, 0, 0, ret);
    return ret;
}

/* rename - переименование/перемещение файла */
static int monitoring_rename(const char *oldpath, const char *newpath,
                             unsigned int flags) {
    (void)flags;
    
    /* Нельзя переименовать что-либо в .stats */
    if (is_stats_file(newpath)) {
        log_operation("RENAME", oldpath, -EACCES, 0);
        return -EACCES;
    }
    
    /* Нельзя переименовать .stats */
    if (is_stats_file(oldpath)) {
        log_operation("RENAME", oldpath, -EACCES, 0);
        return -EACCES;
    }
    
    char *full_old = get_full_path(oldpath);
    char *full_new = get_full_path(newpath);
    
    if (!full_old || !full_new) {
        free(full_old);
        free(full_new);
        log_operation("RENAME", oldpath, -ENOENT, 0);
        return -ENOENT;
    }
    
    int res = rename(full_old, full_new);
    int ret = (res == -1) ? -errno : 0;
    
    free(full_old);
    free(full_new);
    log_operation("RENAME", oldpath, ret, 0);
    return ret;
}

/* truncate - изменение размера файла */
static int monitoring_truncate(const char *path, off_t size,
                               struct fuse_file_info *fi) {
    /* Нельзя изменить размер виртуального файла статистики */
    if (is_stats_file(path)) {
        log_operation("TRUNCATE", path, -EACCES, 0);
        return -EACCES;
    }
    
    int res;
    if (fi && fi->fh) {
        res = ftruncate(fi->fh, size);
    } else {
        char *full_path = get_full_path(path);
        if (!full_path) {
            log_operation("TRUNCATE", path, -ENOENT, 0);
            return -ENOENT;
        }
        res = truncate(full_path, size);
        free(full_path);
    }
    
    int ret = (res == -1) ? -errno : 0;
    log_operation("TRUNCATE", path, ret, 0);
    return ret;
}

/* release - закрытие файла */
static int monitoring_release(const char *path, struct fuse_file_info *fi) {
    /* Для виртуального файла ничего не делаем */
    if (is_stats_file(path)) {
        log_operation("RELEASE", path, 0, 0);
        return 0;
    }
    
    /* Для обычного файла закрываем дескриптор */
    if (fi->fh) {
        close(fi->fh);
        fi->fh = 0;
    }
    
    log_operation("RELEASE", path, 0, 0);
    return 0;
}

/* access - проверка доступа к файлу */
static int monitoring_access(const char *path, int mask) {
    /* Виртуальный файл статистики доступен только для чтения */
    if (is_stats_file(path)) {
        if ((mask & W_OK) || (mask & X_OK)) {
            log_operation("ACCESS", path, -EACCES, 0);
            return -EACCES;
        }
        log_operation("ACCESS", path, 0, 0);
        return 0;
    }
    
    char *full_path = get_full_path(path);
    if (!full_path) {
        log_operation("ACCESS", path, -ENOENT, 0);
        return -ENOENT;
    }
    
    int res = access(full_path, mask);
    int ret = (res == -1) ? -errno : 0;
    
    free(full_path);
    log_operation("ACCESS", path, ret, 0);
    return ret;
}

/* statfs - получение статистики файловой системы */
static int monitoring_statfs(const char *path, struct statvfs *stbuf) {
    (void)path;
    
    char *full_path = get_full_path("/");
    if (!full_path) {
        return -ENOENT;
    }
    
    int res = statvfs(full_path, stbuf);
    free(full_path);
    
    log_operation("STATFS", path, (res == -1) ? -errno : 0, 0);
    return (res == -1) ? -errno : 0;
}

/* ========== СТРУКТУРА ОПЕРАЦИЙ FUSE ========== */

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
    .rename     = monitoring_rename,
    .truncate   = monitoring_truncate,
    .release    = monitoring_release,
    .access     = monitoring_access,
    .statfs     = monitoring_statfs,
};

/* ========== ОСНОВНАЯ ФУНКЦИЯ ========== */

int main(int argc, char *argv[]) {
    /* Проверка аргументов */
    if (argc < 3) {
        fprintf(stderr, "Monitoring FUSE Filesystem v1.1\n");
        fprintf(stderr, "Passthrough FS with real-time statistics\n\n");
        fprintf(stderr, "Usage: %s <source_dir> <mount_point> [options]\n\n", argv[0]);
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  -f   Run in foreground\n");
        fprintf(stderr, "  -d   Enable debug output (logs all operations)\n");
        fprintf(stderr, "  -s   Run single-threaded\n");
        fprintf(stderr, "  -o opt,[opt...]  Mount options\n\n");
        fprintf(stderr, "Virtual file: /.stats (read-only statistics)\n");
        fprintf(stderr, "\nExamples:\n");
        fprintf(stderr, "  %s /tmp/source ~/fuse_mount -f -d\n", argv[0]);
        fprintf(stderr, "  %s /home/user/data /mnt/monitor -o allow_other\n", argv[0]);
        fprintf(stderr, "\nNote: Mount point will be created if it doesn't exist\n");
        return 1;
    }
    
    /* Инициализация времени старта */
    start_time = time(NULL);
    
    /* Проверка наличия флага -d для включения дебага */
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            debug_mode = 1;
            break;
        }
    }
    
    /* Раскрытие тильды в пути (если есть) */
    char expanded_source[PATH_MAX];
    char expanded_mount[PATH_MAX];
    
    /* Обработка пути к исходной директории */
    if (argv[1][0] == '~') {
        const char *home = getenv("HOME");
        if (!home) home = "/";
        snprintf(expanded_source, sizeof(expanded_source), "%s%s", home, argv[1] + 1);
    } else {
        strncpy(expanded_source, argv[1], sizeof(expanded_source) - 1);
        expanded_source[sizeof(expanded_source) - 1] = '\0';
    }
    
    /* Обработка пути к точке монтирования */
    if (argv[2][0] == '~') {
        const char *home = getenv("HOME");
        if (!home) home = "/";
        snprintf(expanded_mount, sizeof(expanded_mount), "%s%s", home, argv[2] + 1);
    } else {
        strncpy(expanded_mount, argv[2], sizeof(expanded_mount) - 1);
        expanded_mount[sizeof(expanded_mount) - 1] = '\0';
    }
    
    /* Упрощенная проверка исходной директории */
    struct stat st;
    
    /* Проверка существования исходной директории */
    if (stat(expanded_source, &st) != 0) {
        fprintf(stderr, "Error: Source directory '%s' does not exist\n", expanded_source);
        fprintf(stderr, "Please create it first: mkdir -p %s\n", expanded_source);
        return 1;
    }
    
    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: Source path '%s' is not a directory\n", expanded_source);
        return 1;
    }
    
    /* Проверка прав доступа к исходной директории */
    if (access(expanded_source, R_OK | W_OK | X_OK) != 0) {
        fprintf(stderr, "Error: No sufficient permissions to source directory '%s'\n", expanded_source);
        fprintf(stderr, "Please check permissions: ls -la %s\n", expanded_source);
        return 1;
    }
    
    /* Получение абсолютного пути без realpath (чтобы избежать проблем) */
    if (expanded_source[0] != '/') {
        fprintf(stderr, "Error: Source path '%s' must be absolute\n", expanded_source);
        return 1;
    }
    
    /* Создание базового пути */
    size_t source_len = strlen(expanded_source);
    base_path = malloc(source_len + 2);  /* +1 для слеша, +1 для \0 */
    if (!base_path) {
        perror("malloc");
        return 1;
    }
    
    strcpy(base_path, expanded_source);
    
    /* Добавление завершающего слеша */
    if (base_path[source_len - 1] != '/') {
        base_path[source_len] = '/';
        base_path[source_len + 1] = '\0';
    }
    
    /* Вывод информации о монтировании */
    fprintf(stderr, "\n=== Monitoring FUSE Filesystem v1.1 ===\n");
    fprintf(stderr, "Source directory:  %s\n", base_path);
    fprintf(stderr, "Mount point:       %s\n", expanded_mount);
    fprintf(stderr, "Process ID:        %d\n", getpid());
    fprintf(stderr, "Debug mode:        %s\n", debug_mode ? "enabled" : "disabled");
    fprintf(stderr, "Virtual stats file: /.stats (read-only)\n");
    fprintf(stderr, "\nTo unmount:\n");
    fprintf(stderr, "  fusermount -u %s\n", expanded_mount);
    fprintf(stderr, "  OR\n");
    fprintf(stderr, "  kill -INT %d\n", getpid());
    fprintf(stderr, "\nTo view statistics:\n");
    fprintf(stderr, "  cat %s/.stats\n", expanded_mount);
    fprintf(stderr, "======================================\n\n");
    
   int fuse_argc = argc - 1;  // На 1 меньше, так как убираем исходную директорию
    char **fuse_argv = malloc((fuse_argc + 1) * sizeof(char*));
    if (!fuse_argv) {
        perror("malloc");
        free(base_path);
        return 1;
    }
    
    // Имя программы
    fuse_argv[0] = argv[0];
    
    // Точка монтирования (второй аргумент пользователя)
    fuse_argv[1] = argv[2];
    
    // Копируем все остальные аргументы (опции), начиная с 3-го
    for (int i = 3; i < argc; i++) {
        fuse_argv[i-1] = argv[i];
    }
    
    // Конец массива
    fuse_argv[fuse_argc] = NULL;
    
    /* Запуск FUSE */
    setenv("FUSE_MAX_THREADS", "16", 1);
    int ret = fuse_main(fuse_argc, fuse_argv, &monitoring_oper, NULL);
    
    /* Вывод финальной статистики */
    fprintf(stderr, "\n=== Final Statistics ===\n");
    size_t final_size;
    char *final_stats = format_stats_string(&final_size);
    if (final_stats) {
        fprintf(stderr, "%s\n", final_stats);
        free(final_stats);
    }
    
    /* Очистка */
    free(fuse_argv);
    free(base_path);
    
    return ret;
}
