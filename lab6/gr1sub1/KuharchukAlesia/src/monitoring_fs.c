#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>

static char *base_path = NULL;

// Структура статистики
static struct {
    int reads, writes, opens, getattrs, readdirs, creates, unlinks, mkdirs;
    size_t bytes_read, bytes_written;
} stats = {0};

// Функция логирования
void log_op(const char* operation, const char* path, int result) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    fprintf(stderr, "[%04d-%02d-%02d %02d:%02d:%02d] %s: %s (result: %d)\n",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
            t->tm_hour, t->tm_min, t->tm_sec,
            operation, path, result);
}

// Функция для получения полного пути
char* get_full_path(const char* base, const char* path) {
    if (!base || !path) return NULL;
    
    size_t base_len = strlen(base);
    size_t path_len = strlen(path);
    char *full_path = malloc(base_len + path_len + 2);
    
    if (!full_path) return NULL;
    
    strcpy(full_path, base);
    
    // Убираем завершающий слеш если есть
    if (base_len > 0 && full_path[base_len-1] == '/') {
        full_path[base_len-1] = '\0';
    }
    
    // Добавляем путь
    if (strcmp(path, "/") != 0) {
        strcat(full_path, path);
    }
    
    return full_path;
}

// Обновление статистики
static inline void update_stats(const char *op, int bytes) {
    if (strcmp(op, "READ") == 0) { 
        stats.reads++; 
        stats.bytes_read += bytes; 
    }
    else if (strcmp(op, "WRITE") == 0) { 
        stats.writes++; 
        stats.bytes_written += bytes; 
    }
    else if (strcmp(op, "OPEN") == 0) stats.opens++;
    else if (strcmp(op, "GETATTR") == 0) stats.getattrs++;
    else if (strcmp(op, "READDIR") == 0) stats.readdirs++;
    else if (strcmp(op, "CREATE") == 0) stats.creates++;
    else if (strcmp(op, "UNLINK") == 0) stats.unlinks++;
    else if (strcmp(op, "MKDIR") == 0) stats.mkdirs++;
}

static int monitor_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void)fi;
    if (!path || !stbuf) return -EINVAL;
    
    memset(stbuf, 0, sizeof(struct stat));

    // Виртуальный файл статистики
    if (strcmp(path, "/.stats") == 0) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = 512;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);
        log_op("GETATTR", path, 0);
        update_stats("GETATTR", 0);
        return 0;
    }

    char *fp = get_full_path(base_path, path);
    if (!fp) return -ENOMEM;
    
    int res = lstat(fp, stbuf);
    if (res == -1) res = -errno;
    
    log_op("GETATTR", path, res);
    update_stats("GETATTR", 0);
    free(fp);
    return res;
}

static int monitor_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                          off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void)offset; (void)fi; (void)flags;
    
    if (!path || !buf || !filler) return -EINVAL;
    
    char *fp = get_full_path(base_path, path);
    if (!fp) return -ENOMEM;
    
    DIR *dp = opendir(fp);
    if (!dp) {
        int res = -errno;
        log_op("READDIR", path, res);
        free(fp);
        return res;
    }

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    // Добавляем виртуальный файл статистики только в корне
    if (strcmp(path, "/") == 0) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_mode = S_IFREG | 0444;
        st.st_size = 512;
        filler(buf, ".stats", &st, 0, 0);
    }

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        // Пропускаем . и .. так как уже добавили
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }
        
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0, 0)) break;
    }

    closedir(dp);
    log_op("READDIR", path, 0);
    update_stats("READDIR", 0);
    free(fp);
    return 0;
}

static int monitor_open(const char *path, struct fuse_file_info *fi) {
    if (!path || !fi) return -EINVAL;
    
    // Виртуальный файл статистики - только чтение
    if (strcmp(path, "/.stats") == 0) {
        if ((fi->flags & O_ACCMODE) != O_RDONLY) {
            log_op("OPEN", path, -EACCES);
            return -EACCES;
        }
        log_op("OPEN", path, 0);
        update_stats("OPEN", 0);
        return 0;
    }

    char *fp = get_full_path(base_path, path);
    if (!fp) return -ENOMEM;
    
    int res = open(fp, fi->flags);
    if (res == -1) {
        res = -errno;
    } else {
        close(res); // Закрываем, так как FUSE сам откроет снова
        res = 0;
    }
    
    log_op("OPEN", path, res);
    update_stats("OPEN", 0);
    free(fp);
    return res;
}

static int monitor_read(const char *path, char *buf, size_t size, off_t offset,
                       struct fuse_file_info *fi) {
    (void)fi;
    
    if (!path || !buf) return -EINVAL;
    
    // Обработка виртуального файла статистики
    if (strcmp(path, "/.stats") == 0) {
        char stat_buf[512];
        int len = snprintf(stat_buf, sizeof(stat_buf),
            "=== File System Statistics ===\n"
            "Read operations: %d\n"
            "Write operations: %d\n"
            "Open operations: %d\n"
            "Create operations: %d\n"
            "Getattr operations: %d\n"
            "Readdir operations: %d\n"
            "Unlink operations: %d\n"
            "Mkdir operations: %d\n"
            "Bytes read: %zu\n"
            "Bytes written: %zu\n"
            "==============================\n",
            stats.reads, stats.writes, stats.opens, stats.creates,
            stats.getattrs, stats.readdirs, stats.unlinks, stats.mkdirs,
            stats.bytes_read, stats.bytes_written);

        if (len < 0) len = 0;
        if (len >= (int)sizeof(stat_buf)) len = sizeof(stat_buf) - 1;
        
        if (offset >= len) return 0;
        if (offset + size > len) size = len - offset;
        
        memcpy(buf, stat_buf + offset, size);
        log_op("READ", path, size);
        update_stats("READ", size);
        return size;
    }

    // Обычные файлы
    char *fp = get_full_path(base_path, path);
    if (!fp) return -ENOMEM;
    
    int fd = open(fp, O_RDONLY);
    if (fd == -1) {
        int res = -errno;
        log_op("READ", path, res);
        free(fp);
        return res;
    }

    int res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;
    
    close(fd);
    log_op("READ", path, res);
    update_stats("READ", res > 0 ? res : 0);
    free(fp);
    return res;
}

static int monitor_write(const char *path, const char *buf, size_t size, off_t offset,
                        struct fuse_file_info *fi) {
    (void)fi;
    
    if (!path || !buf) return -EINVAL;
    
    // Виртуальный файл статистики - запись запрещена
    if (strcmp(path, "/.stats") == 0) {
        log_op("WRITE", path, -EACCES);
        update_stats("WRITE", 0);
        return -EACCES;
    }

    char *fp = get_full_path(base_path, path);
    if (!fp) return -ENOMEM;
    
    int fd = open(fp, O_WRONLY);
    if (fd == -1) {
        int res = -errno;
        log_op("WRITE", path, res);
        free(fp);
        return res;
    }

    int res = pwrite(fd, buf, size, offset);
    if (res == -1) res = -errno;
    
    close(fd);
    log_op("WRITE", path, res);
    update_stats("WRITE", res > 0 ? res : 0);
    free(fp);
    return res;
}

static int monitor_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void)fi;
    
    if (!path) return -EINVAL;

    char *fp = get_full_path(base_path, path);
    if (!fp) return -ENOMEM;
    
    int res = open(fp, O_CREAT | O_WRONLY | O_TRUNC, mode);
    if (res == -1) {
        res = -errno;
    } else {
        close(res);
        res = 0;
    }
    
    log_op("CREATE", path, res);
    update_stats("CREATE", 0);
    free(fp);
    return res;
}

static int monitor_unlink(const char *path) {
    if (!path) return -EINVAL;

    char *fp = get_full_path(base_path, path);
    if (!fp) return -ENOMEM;
    
    int res = unlink(fp);
    if (res == -1) res = -errno;
    
    log_op("UNLINK", path, res);
    update_stats("UNLINK", 0);
    free(fp);
    return res;
}

static int monitor_mkdir(const char *path, mode_t mode) {
    if (!path) return -EINVAL;

    char *fp = get_full_path(base_path, path);
    if (!fp) return -ENOMEM;
    
    int res = mkdir(fp, mode);
    if (res == -1) res = -errno;
    
    log_op("MKDIR", path, res);
    update_stats("MKDIR", 0);
    free(fp);
    return res;
}

static struct fuse_operations monitor_ops = {
    .getattr = monitor_getattr,
    .readdir = monitor_readdir,
    .open = monitor_open,
    .read = monitor_read,
    .write = monitor_write,
    .create = monitor_create,
    .unlink = monitor_unlink,
    .mkdir = monitor_mkdir,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mount_point> [FUSE options]\n", argv[0]);
        fprintf(stderr, "Example: %s /tmp/source /mnt/monitor -f\n", argv[0]);
        return 1;
    }

    char *temp_base = realpath(argv[1], NULL);
    if (!temp_base) {
        fprintf(stderr, "Error: Invalid source directory '%s'\n", argv[1]);
        return 1;
    }
    
    // Проверяем что директория существует
    struct stat st;
    if (stat(temp_base, &st) == -1 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: Cannot access source directory '%s'\n", temp_base);
        free(temp_base);
        return 1;
    }

    base_path = temp_base;
    
    fprintf(stderr, "Monitoring FS: %s -> %s\n", base_path, argv[2]);
    fprintf(stderr, "Statistics available at: /.stats\n");

    // Подготавливаем аргументы для FUSE
    argv[1] = argv[2];
    int ret = fuse_main(argc - 1, argv + 1, &monitor_ops, NULL);
    
    free(base_path);
    return ret;
}
