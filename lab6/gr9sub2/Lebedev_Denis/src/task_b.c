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
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>

static char *base_dir_path;
#define MAX_PATH_LEN 4096

/**
 * @brief Логирует выполненную операцию в stderr.
 *
 * @param op Название операции (e.g., "GETATTR").
 * @param path Относительный путь к файлу/директории.
 * @param result Результат операции (0 для успеха, -errno для ошибки).
 */
static void log_operation(const char *op, const char *path, int result) {
    time_t now = time(NULL);
    char timestamp[100];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    if (result < 0) {
        fprintf(stderr, "[%s] %s: %s (result: %d %s)\n", timestamp, op, path, result, strerror(-result));
    } else {
        fprintf(stderr, "[%s] %s: %s (result: %d)\n", timestamp, op, path, result);
    }
}

/**
 * @brief Преобразует относительный путь FUSE в полный путь в реальной ФС.
 *
 * @param fullpath Буфер для записи полного пути.
 * @param path Относительный путь, полученный от FUSE.
 */
static void get_full_path(char fullpath[MAX_PATH_LEN], const char *path) {
    snprintf(fullpath, MAX_PATH_LEN, "%s%s", base_dir_path, path);
}

/**
 * @brief Применяет преобразование ROT13 к строке.
 *
 * @param str Строка для преобразования.
 * @param len Длина строки.
 */
static void rot13_transform(char *str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        if (c >= 'a' && c <= 'z') {
            str[i] = 'a' + (c - 'a' + 13) % 26;
        } else if (c >= 'A' && c <= 'Z') {
            str[i] = 'A' + (c - 'A' + 13) % 26;
        }
        // Остальные символы (цифры, знаки препинания) не изменяются
    }
}


// --- Реализация операций FUSE ---

static int passthrough_getattr(const char *path, struct stat *stbuf,
                               struct fuse_file_info *fi) {
    (void) fi;
    char fullpath[MAX_PATH_LEN];
    get_full_path(fullpath, path);

    int res = lstat(fullpath, stbuf);
    log_operation("GETATTR", path, res == -1 ? -errno : 0);
    if (res == -1) {
        return -errno;
    }
    return 0;
}

static int passthrough_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                               off_t offset, struct fuse_file_info *fi,
                               enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    char fullpath[MAX_PATH_LEN];
    get_full_path(fullpath, path);

    DIR *dp = opendir(fullpath);
    if (dp == NULL) {
        int res = -errno;
        log_operation("READDIR", path, res);
        return res;
    }

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0, 0)) {
            break;
        }
    }

    closedir(dp);
    log_operation("READDIR", path, 0);
    return 0;
}

static int passthrough_open(const char *path, struct fuse_file_info *fi) {
    char fullpath[MAX_PATH_LEN];
    get_full_path(fullpath, path);

    int fd = open(fullpath, fi->flags);
    if (fd == -1) {
        int res = -errno;
        log_operation("OPEN", path, res);
        return res;
    }
    // Мы не храним fd в fi->fh, так как это passthrough.
    // Ядро кэширует права доступа, а read/write будут открывать файл заново.
    close(fd);
    log_operation("OPEN", path, 0);
    return 0;
}

static int passthrough_read(const char *path, char *buf, size_t size, off_t offset,
                            struct fuse_file_info *fi) {
    (void) fi;
    char fullpath[MAX_PATH_LEN];
    get_full_path(fullpath, path);

    int fd = open(fullpath, O_RDONLY);
    if (fd == -1) {
        int res = -errno;
        log_operation("READ", path, res);
        return res;
    }

    int res = pread(fd, buf, size, offset);
    if (res == -1) {
        res = -errno;
    }
    close(fd);

    // Если данные успешно прочитаны, расшифровываем их
    if (res > 0) {
        rot13_transform(buf, res);
    }

    log_operation("READ", path, res);
    return res;
}

static int passthrough_write(const char *path, const char *buf, size_t size,
                             off_t offset, struct fuse_file_info *fi) {
    (void) fi;
    char fullpath[MAX_PATH_LEN];
    get_full_path(fullpath, path);

    // Создаем временный буфер для зашифрованных данных
    char *encrypted_buf = malloc(size);
    if (encrypted_buf == NULL) {
        return -ENOMEM;
    }
    memcpy(encrypted_buf, buf, size);

    // Шифруем данные
    rot13_transform(encrypted_buf, size);

    int fd = open(fullpath, O_WRONLY);
    if (fd == -1) {
        int res = -errno;
        log_operation("WRITE", path, res);
        free(encrypted_buf);
        return res;
    }

    int res = pwrite(fd, encrypted_buf, size, offset);
    if (res == -1) {
        res = -errno;
    }
    close(fd);
    free(encrypted_buf);

    log_operation("WRITE", path, res);
    return res;
}

static int passthrough_create(const char *path, mode_t mode,
                              struct fuse_file_info *fi) {
    (void)fi;
    char fullpath[MAX_PATH_LEN];
    get_full_path(fullpath, path);

    int fd = creat(fullpath, mode);
    if (fd == -1) {
        int res = -errno;
        log_operation("CREATE", path, res);
        return res;
    }

    close(fd);
    log_operation("CREATE", path, 0);
    return 0;
}

static int passthrough_unlink(const char *path) {
    char fullpath[MAX_PATH_LEN];
    get_full_path(fullpath, path);

    int res = unlink(fullpath);
    if (res == -1) {
        res = -errno;
        log_operation("UNLINK", path, res);
        return res;
    }

    log_operation("UNLINK", path, 0);
    return 0;
}

static int passthrough_mkdir(const char *path, mode_t mode) {
    char fullpath[MAX_PATH_LEN];
    get_full_path(fullpath, path);

    int res = mkdir(fullpath, mode);
    if (res == -1) {
        res = -errno;
        log_operation("MKDIR", path, res);
        return res;
    }

    log_operation("MKDIR", path, 0);
    return 0;
}

static int passthrough_rmdir(const char *path) {
    char fullpath[MAX_PATH_LEN];
    get_full_path(fullpath, path);

    int res = rmdir(fullpath);
    if (res == -1) {
        res = -errno;
        log_operation("RMDIR", path, res);
        return res;
    }

    log_operation("RMDIR", path, 0);
    return 0;
}

static const struct fuse_operations passthrough_oper = {
    .getattr    = passthrough_getattr,
    .readdir    = passthrough_readdir,
    .open       = passthrough_open,
    .read       = passthrough_read,
    .write      = passthrough_write,
    .create     = passthrough_create,
    .unlink     = passthrough_unlink,
    .mkdir      = passthrough_mkdir,
    .rmdir      = passthrough_rmdir,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mount_point> [fuse_options]\n", argv[0]);
        return 1;
    }

    base_dir_path = realpath(argv[1], NULL);
    if (base_dir_path == NULL) {
        perror("realpath");
        return 1;
    }

    // FUSE получает все аргументы, но мы должны "удалить" наш первый аргумент (source_dir)
    // argv[0] - имя программы
    // argv[1] - source_dir (не нужен FUSE)
    // argv[2] - mount_point
    // argv[3..] - опции FUSE
    argv[1] = argv[2];
    for (int i = 2; i < argc - 1; i++) {
        argv[i] = argv[i+1];
    }
    argc--;

    fprintf(stderr, "Starting FUSE passthrough for %s\n", base_dir_path);

    int ret = fuse_main(argc, argv, &passthrough_oper, NULL);

    free(base_dir_path);
    return ret;
}
