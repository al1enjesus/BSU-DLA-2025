#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>

static char *base = NULL;
static long reads = 0, writes = 0, opens = 0;
static long bytes_read = 0, bytes_written = 0;

// Построение полного пути
static void fullpath(char *buf, const char *path) {
    snprintf(buf, 1024, "%s%s", base, path);
}

// Получение атрибутов файла
static int fs_getattr(const char *path, struct stat *st, struct fuse_file_info *fi) {
    (void)fi;
    memset(st, 0, sizeof(*st));

    if (strcmp(path, "/.stats") == 0) {
        st->st_mode = S_IFREG | 0444;
        st->st_nlink = 1;
        st->st_size = 256;
        st->st_uid = getuid();
        st->st_gid = getgid();
        st->st_atime = time(NULL);
        st->st_mtime = time(NULL);
        st->st_ctime = time(NULL);
        return 0;
    }

    char f[1024];
    fullpath(f, path);
    if (lstat(f, st) == -1) return -errno;
    return 0;
}

// Чтение содержимого директории
static int fs_readdir(const char *p, void *b, fuse_fill_dir_t f,
                      off_t o, struct fuse_file_info *fi,
                      enum fuse_readdir_flags fl) {
    (void)o;
    (void)fi;
    (void)fl;
    
    f(b, ".", NULL, 0, 0);
    f(b, "..", NULL, 0, 0);
    f(b, ".stats", NULL, 0, 0);

    if (strcmp(p, "/") == 0) {
        char fp[1024];
        fullpath(fp, p);

        DIR *d = opendir(fp);
        if (!d) return -errno;

        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
                continue;
            f(b, de->d_name, NULL, 0, 0);
        }

        closedir(d);
    }
    
    return 0;
}

// Открытие файла
static int fs_open(const char *p, struct fuse_file_info *fi) {
    opens++;
    
    if (strcmp(p, "/.stats") == 0) {
        if ((fi->flags & O_ACCMODE) != O_RDONLY) {
            return -EACCES;
        }
        return 0;
    }

    char f[1024];
    fullpath(f, p);
    int fd = open(f, fi->flags);
    if (fd == -1) return -errno;
    close(fd);
    return 0;
}

// Чтение из файла
static int fs_read(const char *path, char *buf, size_t size,
                   off_t offset, struct fuse_file_info *fi) {
    reads++;

    if (strcmp(path, "/.stats") == 0) {
        char stats[256];
        int n = snprintf(stats, sizeof(stats),
            "reads: %ld\n"
            "writes: %ld\n"
            "opens: %ld\n"
            "bytes_read: %ld\n"
            "bytes_written: %ld\n",
            reads, writes, opens, bytes_read, bytes_written);
        
        if (offset >= n) return 0;
        if (offset + size > n) size = n - offset;
        
        memcpy(buf, stats + offset, size);
        return size;
    }

    char f[1024];
    fullpath(f, path);
    int fd = open(f, O_RDONLY);
    if (fd == -1) return -errno;

    int res = pread(fd, buf, size, offset);
    if (res > 0) bytes_read += res;
    close(fd);
    return res;
}

// Запись в файл
static int fs_write(const char *path, const char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi) {
    writes++;

    if (strcmp(path, "/.stats") == 0) {
        return -EACCES;
    }

    char f[1024];
    fullpath(f, path);

    int fd = open(f, O_WRONLY);
    if (fd == -1) return -errno;

    int res = pwrite(fd, buf, size, offset);
    if (res > 0) bytes_written += res;

    close(fd);
    return res;
}

// Создание файла
static int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char f[1024];
    fullpath(f, path);
    
    int fd = creat(f, mode);
    if (fd == -1) return -errno;
    
    close(fd);
    return 0;
}

// Удаление файла
static int fs_unlink(const char *path) {
    char f[1024];
    fullpath(f, path);
    
    if (unlink(f) == -1) return -errno;
    return 0;
}

// Создание директории
static int fs_mkdir(const char *path, mode_t mode) {
    char f[1024];
    fullpath(f, path);
    
    if (mkdir(f, mode) == -1) return -errno;
    return 0;
}

// Удаление директории
static int fs_rmdir(const char *path) {
    char f[1024];
    fullpath(f, path);
    
    if (rmdir(f) == -1) return -errno;
    return 0;
}

// Операции FUSE
static struct fuse_operations ops = {
    .getattr = fs_getattr,
    .readdir = fs_readdir,
    .open    = fs_open,
    .read    = fs_read,
    .write   = fs_write,
    .create  = fs_create,
    .unlink  = fs_unlink,
    .mkdir   = fs_mkdir,
    .rmdir   = fs_rmdir,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "usage: monitoring_fs <src> <mount>\n");
        return 1;
    }

    base = realpath(argv[1], NULL);
    if (base == NULL) {
        perror("realpath");
        return 1;
    }

    char *fuse_argv[4];
    fuse_argv[0] = argv[0];
    fuse_argv[1] = argv[2];
    fuse_argv[2] = "-f";
    fuse_argv[3] = NULL;
    
    int ret = fuse_main(3, fuse_argv, &ops, NULL);
    
    free(base);
    return ret;
}