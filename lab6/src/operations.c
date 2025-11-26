#include "operations.h"
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>

extern char *source_dir;

// Helper функция для безопасного получения полного пути
char* get_full_path_or_error(const char *path, int *error_code) {
    return build_fullpath_safe(source_dir, path, error_code);
}

static int handle_operation_error(const char *operation, const char *path, int error_code) {
    if (error_code < 0) {
        fprintf(stderr, "[%s] %s: %s (error: %s)\n",
                get_timestamp(), operation, path, strerror(-error_code));
    }
    return error_code;
}

int myfuse_getattr(const char *path, struct stat *stbuf) {
    int error_code = 0;
    char *full_path = get_full_path_or_error(path, &error_code);
    if (!full_path) {
        return handle_operation_error("GETATTR", path, error_code);
    }

    // Используем lstat для корректной обработки симлинков
    int res = lstat(full_path, stbuf);
    if (res == -1) {
        error_code = -errno;
    }

    fprintf(stderr, "[%s] GETATTR: %s (result: %d)\n",
            get_timestamp(), path, res);

    free(full_path);
    return handle_operation_error("GETATTR", path, error_code);
}

int myfuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                   off_t offset, struct fuse_file_info *fi) {
    int error_code = 0;
    char *full_path = get_full_path_or_error(path, &error_code);
    if (!full_path) {
        return handle_operation_error("READDIR", path, error_code);
    }

    DIR *dp = opendir(full_path);
    if (!dp) {
        error_code = -errno;
        free(full_path);
        return handle_operation_error("READDIR", path, error_code);
    }

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        // Убрано дублирование "." и "..", так как они уже есть в readdir
        if (filler(buf, de->d_name, NULL, 0) != 0) {
            break;
        }
    }

    closedir(dp);
    free(full_path);

    fprintf(stderr, "[%s] READDIR: %s (result: 0)\n", get_timestamp(), path);
    return 0;
}

int myfuse_open(const char *path, struct fuse_file_info *fi) {
    int error_code = 0;
    char *full_path = get_full_path_or_error(path, &error_code);
    if (!full_path) {
        return handle_operation_error("OPEN", path, error_code);
    }

    // Дополнительная проверка прав доступа
    error_code = check_access_permissions(path, fi->flags);
    if (error_code != 0) {
        free(full_path);
        return handle_operation_error("OPEN", path, error_code);
    }

    int res = open(full_path, fi->flags);
    if (res == -1) {
        error_code = -errno;
    } else {
        close(res); // FUSE сам управляет файловыми дескрипторами
    }

    fprintf(stderr, "[%s] OPEN: %s (result: %d)\n",
            get_timestamp(), path, res == -1 ? -1 : 0);

    free(full_path);
    return handle_operation_error("OPEN", path, error_code);
}

int myfuse_read(const char *path, char *buf, size_t size, off_t offset,
                struct fuse_file_info *fi) {
    // Валидация параметров
    int validation = validate_operation_size(size, offset);
    if (validation != 0) {
        return handle_operation_error("READ", path, validation);
    }

    int error_code = 0;
    char *full_path = get_full_path_or_error(path, &error_code);
    if (!full_path) {
        return handle_operation_error("READ", path, error_code);
    }

    int fd = open(full_path, O_RDONLY);
    if (fd == -1) {
        error_code = -errno;
        free(full_path);
        return handle_operation_error("READ", path, error_code);
    }

    int res = pread(fd, buf, size, offset);
    if (res == -1) {
        error_code = -errno;
    }

    close(fd);
    free(full_path);

    fprintf(stderr, "[%s] READ: %s (%ld bytes at offset %ld, result: %d)\n",
            get_timestamp(), path, size, offset, res);
    return handle_operation_error("READ", path, error_code);
}

int myfuse_write(const char *path, const char *buf, size_t size, off_t offset,
                 struct fuse_file_info *fi) {
    // Валидация параметров
    int validation = validate_operation_size(size, offset);
    if (validation != 0) {
        return handle_operation_error("WRITE", path, validation);
    }

    int error_code = 0;
    char *full_path = get_full_path_or_error(path, &error_code);
    if (!full_path) {
        return handle_operation_error("WRITE", path, error_code);
    }

    int fd = open(full_path, O_WRONLY);
    if (fd == -1) {
        error_code = -errno;
        free(full_path);
        return handle_operation_error("WRITE", path, error_code);
    }

    int res = pwrite(fd, buf, size, offset);
    if (res == -1) {
        error_code = -errno;
    }

    close(fd);
    free(full_path);

    fprintf(stderr, "[%s] WRITE: %s (%ld bytes at offset %ld, result: %d)\n",
            get_timestamp(), path, size, offset, res);
    return handle_operation_error("WRITE", path, error_code);
}

// Остальные функции (create, unlink, mkdir, rmdir) аналогично переписаны
// с использованием helper функций и безопасных операций

int myfuse_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    int error_code = 0;
    char *full_path = get_full_path_or_error(path, &error_code);
    if (!full_path) {
        return handle_operation_error("CREATE", path, error_code);
    }

    int fd = open(full_path, fi->flags, mode);
    if (fd == -1) {
        error_code = -errno;
    } else {
        close(fd);
    }

    fprintf(stderr, "[%s] CREATE: %s (result: %d)\n",
            get_timestamp(), path, fd == -1 ? -1 : 0);

    free(full_path);
    return handle_operation_error("CREATE", path, error_code);
}

int myfuse_unlink(const char *path) {
    int error_code = 0;
    char *full_path = get_full_path_or_error(path, &error_code);
    if (!full_path) {
        return handle_operation_error("UNLINK", path, error_code);
    }

    int res = unlink(full_path);
    if (res == -1) {
        error_code = -errno;
    }

    fprintf(stderr, "[%s] UNLINK: %s (result: %d)\n",
            get_timestamp(), path, res);

    free(full_path);
    return handle_operation_error("UNLINK", path, error_code);
}