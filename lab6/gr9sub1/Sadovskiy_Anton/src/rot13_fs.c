#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>

static const char *source_dir = NULL;

// ROT13 преобразование
static void rot13_transform(char *buf, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        char c = buf[i];
        if (c >= 'A' && c <= 'Z')
        {
            buf[i] = 'A' + (c - 'A' + 13) % 26;
        }
        else if (c >= 'a' && c <= 'z')
        {
            buf[i] = 'a' + (c - 'a' + 13) % 26;
        }
        // Остальные символы не изменяются
    }
}

static void get_full_path(char *fullpath, const char *path)
{
    strcpy(fullpath, source_dir);
    strncat(fullpath, path, PATH_MAX - strlen(source_dir));
}

static void log_operation(const char *op, const char *path, int result)
{
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(stderr, "[%s] %s: %s (result: %d)\n", timestamp, op, path, result);
}

static int rot13_getattr(const char *path, struct stat *stbuf)
{
    char fullpath[PATH_MAX];
    get_full_path(fullpath, path);

    int res = lstat(fullpath, stbuf);
    log_operation("GETATTR", path, res == 0 ? 0 : -errno);

    if (res == -1)
        return -errno;

    return 0;
}

static int rot13_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi)
{
    (void)offset;
    (void)fi;

    char fullpath[PATH_MAX];
    get_full_path(fullpath, path);

    DIR *dp = opendir(fullpath);
    if (dp == NULL)
    {
        log_operation("READDIR", path, -errno);
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
    log_operation("READDIR", path, 0);
    return 0;
}

static int rot13_open(const char *path, struct fuse_file_info *fi)
{
    char fullpath[PATH_MAX];
    get_full_path(fullpath, path);

    int fd = open(fullpath, fi->flags);
    if (fd == -1)
    {
        log_operation("OPEN", path, -errno);
        return -errno;
    }

    close(fd);
    log_operation("OPEN", path, 0);
    return 0;
}

// Чтение с расшифровкой ROT13
static int rot13_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi)
{
    (void)fi;

    char fullpath[PATH_MAX];
    get_full_path(fullpath, path);

    int fd = open(fullpath, O_RDONLY);
    if (fd == -1)
    {
        log_operation("READ", path, -errno);
        return -errno;
    }

    // Читаем зашифрованные данные
    int res = pread(fd, buf, size, offset);
    if (res == -1)
    {
        res = -errno;
    }
    else
    {
        // Расшифровываем ROT13 (ROT13 обратим сам к себе)
        rot13_transform(buf, res);
    }

    close(fd);
    log_operation("READ", path, res);
    return res;
}

// Запись с шифрованием ROT13
static int rot13_write(const char *path, const char *buf, size_t size,
                       off_t offset, struct fuse_file_info *fi)
{
    (void)fi;

    char fullpath[PATH_MAX];
    get_full_path(fullpath, path);

    // Копируем буфер для шифрования
    char *encrypted = malloc(size);
    if (encrypted == NULL)
    {
        return -ENOMEM;
    }

    memcpy(encrypted, buf, size);
    rot13_transform(encrypted, size);

    int fd = open(fullpath, O_WRONLY);
    if (fd == -1)
    {
        free(encrypted);
        log_operation("WRITE", path, -errno);
        return -errno;
    }

    // Записываем зашифрованные данные
    int res = pwrite(fd, encrypted, size, offset);
    if (res == -1)
        res = -errno;

    close(fd);
    free(encrypted);
    log_operation("WRITE", path, res);
    return res;
}

static int rot13_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    char fullpath[PATH_MAX];
    get_full_path(fullpath, path);

    int fd = open(fullpath, fi->flags, mode);
    if (fd == -1)
    {
        log_operation("CREATE", path, -errno);
        return -errno;
    }

    close(fd);
    log_operation("CREATE", path, 0);
    return 0;
}

static int rot13_unlink(const char *path)
{
    char fullpath[PATH_MAX];
    get_full_path(fullpath, path);

    int res = unlink(fullpath);
    log_operation("UNLINK", path, res == 0 ? 0 : -errno);

    if (res == -1)
        return -errno;

    return 0;
}

static int rot13_mkdir(const char *path, mode_t mode)
{
    char fullpath[PATH_MAX];
    get_full_path(fullpath, path);

    int res = mkdir(fullpath, mode);
    log_operation("MKDIR", path, res == 0 ? 0 : -errno);

    if (res == -1)
        return -errno;

    return 0;
}

static int rot13_rmdir(const char *path)
{
    char fullpath[PATH_MAX];
    get_full_path(fullpath, path);

    int res = rmdir(fullpath);
    log_operation("RMDIR", path, res == 0 ? 0 : -errno);

    if (res == -1)
        return -errno;

    return 0;
}

static struct fuse_operations rot13_oper = {
    .getattr = rot13_getattr,
    .readdir = rot13_readdir,
    .open = rot13_open,
    .read = rot13_read,
    .write = rot13_write,
    .create = rot13_create,
    .unlink = rot13_unlink,
    .mkdir = rot13_mkdir,
    .rmdir = rot13_rmdir,
};

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s <source_dir> <mountpoint> [FUSE options]\n", argv[0]);
        fprintf(stderr, "ROT13 Encryption Filesystem\n");
        fprintf(stderr, "Files are encrypted with ROT13 on disk, decrypted on read\n");
        return 1;
    }

    source_dir = realpath(argv[1], NULL);
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

    fprintf(stderr, "Mounting ROT13 FS: %s at %s\n", source_dir, argv[1]);

    int ret = fuse_main(argc, argv, &rot13_oper, NULL);

    free((void *)source_dir);
    return ret;
}