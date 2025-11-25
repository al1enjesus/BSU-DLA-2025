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
#include <sys/stat.h>
#include <time.h>

#include "fuse_common.h"

// Базовая директория для passthrough
// Базовая директория для passthrough
static char *source_dir = NULL;

// Получение атрибутов файла
static int passthrough_getattr(const char *path, struct stat *stbuf)
{
    char fullpath[PATH_MAX];
    int ret = get_full_path_safe(fullpath, source_dir, path);
    if (ret != 0)
    {
        log_operation_safe("GETATTR", path, ret);
        return ret;
    }

    int res = lstat(fullpath, stbuf);
    log_operation_safe("GETATTR", path, res == 0 ? 0 : -errno);

    if (res == -1)
        return -errno;

    return 0;
}

// Чтение содержимого директории
static int passthrough_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                               off_t offset, struct fuse_file_info *fi)
{
    (void)offset;
    (void)fi;

    char fullpath[PATH_MAX];
    int ret = get_full_path_safe(fullpath, source_dir, path);
    if (ret != 0)
    {
        log_operation_safe("READDIR", path, ret);
        return ret;
    }

    DIR *dp = opendir(fullpath);
    if (dp == NULL)
    {
        log_operation_safe("READDIR", path, -errno);
        return -errno;
    }

    struct dirent *de;
    while ((de = readdir(dp)) != NULL)
    {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;

        if (filler(buf, de->d_name, &st, 0, 0))
            break;
    }

    closedir(dp);
    log_operation_safe("READDIR", path, 0);
    return 0;
}

// Открытие файла
static int passthrough_open(const char *path, struct fuse_file_info *fi)
{
    char fullpath[PATH_MAX];
    int ret = get_full_path_safe(fullpath, source_dir, path);
    if (ret != 0)
    {
        log_operation_safe("OPEN", path, ret);
        return ret;
    }

    int fd = open(fullpath, fi->flags);
    if (fd == -1)
    {
        log_operation_safe("OPEN", path, -errno);
        return -errno;
    }

    close(fd);
    log_operation_safe("OPEN", path, 0);
    return 0;
}

// Чтение из файла
static int passthrough_read(const char *path, char *buf, size_t size, off_t offset,
                            struct fuse_file_info *fi)
{
    (void)fi;

    char fullpath[PATH_MAX];
    int ret = get_full_path_safe(fullpath, source_dir, path);
    if (ret != 0)
    {
        log_operation_safe("READ", path, ret);
        return ret;
    }

    int fd = open(fullpath, O_RDONLY);
    if (fd == -1)
    {
        log_operation_safe("READ", path, -errno);
        return -errno;
    }

    int res = pread(fd, buf, size, offset);
    if (res == -1)
        res = -errno;

    close(fd);
    log_operation_safe("READ", path, res);
    return res;
}

// Запись в файл
static int passthrough_write(const char *path, const char *buf, size_t size,
                             off_t offset, struct fuse_file_info *fi)
{
    (void)fi;

    char fullpath[PATH_MAX];
    int ret = get_full_path_safe(fullpath, source_dir, path);
    if (ret != 0)
    {
        log_operation_safe("WRITE", path, ret);
        return ret;
    }

    int fd = open(fullpath, O_WRONLY);
    if (fd == -1)
    {
        log_operation_safe("WRITE", path, -errno);
        return -errno;
    }

    int res = pwrite(fd, buf, size, offset);
    if (res == -1)
        res = -errno;

    close(fd);
    log_operation_safe("WRITE", path, res);
    return res;
}

// Создание файла
static int passthrough_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    char fullpath[PATH_MAX];
    int ret = get_full_path_safe(fullpath, source_dir, path);
    if (ret != 0)
    {
        log_operation_safe("CREATE", path, ret);
        return ret;
    }

    int fd = open(fullpath, fi->flags, mode);
    if (fd == -1)
    {
        log_operation_safe("CREATE", path, -errno);
        return -errno;
    }

    close(fd);
    log_operation_safe("CREATE", path, 0);
    return 0;
}

// Удаление файла
static int passthrough_unlink(const char *path)
{
    char fullpath[PATH_MAX];
    int ret = get_full_path_safe(fullpath, source_dir, path);
    if (ret != 0)
    {
        log_operation_safe("UNLINK", path, ret);
        return ret;
    }

    int res = unlink(fullpath);
    log_operation_safe("UNLINK", path, res == 0 ? 0 : -errno);

    if (res == -1)
        return -errno;

    return 0;
}

// Создание директории
static int passthrough_mkdir(const char *path, mode_t mode)
{
    char fullpath[PATH_MAX];
    int ret = get_full_path_safe(fullpath, source_dir, path);
    if (ret != 0)
    {
        log_operation_safe("MKDIR", path, ret);
        return ret;
    }

    int res = mkdir(fullpath, mode);
    log_operation_safe("MKDIR", path, res == 0 ? 0 : -errno);

    if (res == -1)
        return -errno;

    return 0;
}

// Удаление директории
static int passthrough_rmdir(const char *path)
{
    char fullpath[PATH_MAX];
    int ret = get_full_path_safe(fullpath, source_dir, path);
    if (ret != 0)
    {
        log_operation_safe("RMDIR", path, ret);
        return ret;
    }

    int res = rmdir(fullpath);
    log_operation_safe("RMDIR", path, res == 0 ? 0 : -errno);

    if (res == -1)
        return -errno;

    return 0;
}

// Структура операций FUSE
static struct fuse_operations passthrough_oper = {
    .getattr = passthrough_getattr,
    .readdir = passthrough_readdir,
    .open = passthrough_open,
    .read = passthrough_read,
    .write = passthrough_write,
    .create = passthrough_create,
    .unlink = passthrough_unlink,
    .mkdir = passthrough_mkdir,
    .rmdir = passthrough_rmdir,
};

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s <source_dir> <mountpoint> [FUSE options]\n", argv[0]);
        return 1;
    }

    source_dir = validate_source_dir(argv[1]);
    if (source_dir == NULL)
    {
        perror("Invalid source directory");
        return 1;
    }

    // Shift argv left (remove source_dir)
    for (int i = 1; i < argc - 1; i++)
    {
        argv[i] = argv[i + 1];
    }
    argc--;

    fprintf(stderr, "Mounting %s at %s\n", source_dir, argv[1]);

    int ret = fuse_main(argc, argv, &passthrough_oper, NULL);

    free(source_dir);
    return ret;
}