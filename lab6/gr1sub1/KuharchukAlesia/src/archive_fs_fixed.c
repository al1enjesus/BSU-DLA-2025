#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static char *archive_path = NULL;

// Структура для хранения информации о файле
typedef struct {
    char name[256];
    size_t size;
    mode_t mode;
    time_t mtime;
    off_t offset;
} archive_file_t;

static archive_file_t files[100];
static int file_count = 0;

// Функция для парсинга tar архива
int parse_tar_archive(const char *archive_path) {
    int fd = open(archive_path, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Error: Cannot open archive file %s\n", archive_path);
        return -1;
    }

    off_t offset = 0;
    file_count = 0;

    while (1) {
        char header[512];
        ssize_t bytes_read = pread(fd, header, sizeof(header), offset);
        
        if (bytes_read != sizeof(header) || header[0] == '\0') {
            break;
        }

        // Пропускаем пустые блоки
        if (strlen(header) == 0) {
            offset += 512;
            continue;
        }

        // Извлекаем имя файла
        char name[256];
        strncpy(name, header, 100);
        name[99] = '\0';
        
        // Убираем завершающий слеш для директорий
        if (name[strlen(name)-1] == '/') {
            name[strlen(name)-1] = '\0';
        }

        // Пропускаем текущую директорию
        if (strcmp(name, ".") == 0 || strlen(name) == 0) {
            offset += 512;
            continue;
        }

        // Парсим размер файла (octal to decimal)
        char size_str[12];
        strncpy(size_str, header + 124, 11);
        size_str[11] = '\0';
        size_t size = strtoul(size_str, NULL, 8);

        // Сохраняем только обычные файлы (не директории)
        if (size > 0 && header[156] == '0') {
            strncpy(files[file_count].name, name, sizeof(files[file_count].name) - 1);
            files[file_count].size = size;
            files[file_count].offset = offset + 512;
            
            printf("Found file: %s, size: %zu, offset: %ld\n", 
                   files[file_count].name, files[file_count].size, 
                   (long)files[file_count].offset);

            file_count++;
        }
        
        // Переходим к следующему файлу (выравнивание по 512 байт)
        offset += 512 + ((size + 511) / 512) * 512;
        
        if (file_count >= 100) break;
    }

    close(fd);
    printf("Total files found: %d\n", file_count);
    return 0;
}

// Функция логирования
void log_op(const char* operation, const char* path, int result) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    fprintf(stderr, "[%04d-%02d-%02d %02d:%02d:%02d] %s: %s (result: %d)\n",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
            t->tm_hour, t->tm_min, t->tm_sec,
            operation, path, result);
}

static int archive_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void)fi;
    
    if (!path || !stbuf) return -EINVAL;
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        stbuf->st_size = 4096;
    } else {
        // Ищем файл в архиве
        const char *filename = path + 1; // Пропускаем первый слеш
        int found = 0;
        
        for (int i = 0; i < file_count; i++) {
            if (strcmp(files[i].name, filename) == 0) {
                stbuf->st_mode = S_IFREG | 0444;
                stbuf->st_nlink = 1;
                stbuf->st_size = files[i].size;
                stbuf->st_mtime = time(NULL);
                found = 1;
                break;
            }
        }
        
        if (!found) {
            log_op("ARCHIVE_GETATTR", path, -ENOENT);
            return -ENOENT;
        }
    }

    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_atime = stbuf->st_ctime = time(NULL);
    
    log_op("ARCHIVE_GETATTR", path, 0);
    return 0;
}

static int archive_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                          off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void)offset; (void)fi; (void)flags;

    if (!path || !buf || !filler) return -EINVAL;

    if (strcmp(path, "/") != 0) {
        log_op("ARCHIVE_READDIR", path, -ENOTDIR);
        return -ENOTDIR;
    }

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    // Добавляем реальные файлы из архива
    for (int i = 0; i < file_count; i++) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_mode = S_IFREG | 0444;
        st.st_size = files[i].size;
        filler(buf, files[i].name, &st, 0, 0);
    }

    log_op("ARCHIVE_READDIR", path, 0);
    return 0;
}

static int archive_open(const char *path, struct fuse_file_info *fi) {
    if (!path || !fi) return -EINVAL;

    if (strcmp(path, "/") == 0) {
        log_op("ARCHIVE_OPEN", path, -EISDIR);
        return -EISDIR;
    }

    // Проверяем существование файла
    const char *filename = path + 1;
    int found = 0;
    for (int i = 0; i < file_count; i++) {
        if (strcmp(files[i].name, filename) == 0) {
            found = 1;
            break;
        }
    }

    if (!found) {
        log_op("ARCHIVE_OPEN", path, -ENOENT);
        return -ENOENT;
    }

    // Только для чтения
    if ((fi->flags & O_ACCMODE) != O_RDONLY) {
        log_op("ARCHIVE_OPEN", path, -EACCES);
        return -EACCES;
    }

    log_op("ARCHIVE_OPEN", path, 0);
    return 0;
}

static int archive_read(const char *path, char *buf, size_t size, off_t offset,
                       struct fuse_file_info *fi) {
    (void)fi;

    if (!path || !buf) return -EINVAL;

    const char *filename = path + 1;
    int file_index = -1;
    
    // Ищем файл в архиве
    for (int i = 0; i < file_count; i++) {
        if (strcmp(files[i].name, filename) == 0) {
            file_index = i;
            break;
        }
    }

    if (file_index == -1) {
        log_op("ARCHIVE_READ", path, -ENOENT);
        return -ENOENT;
    }

    // Проверяем границы
    if (offset >= files[file_index].size) {
        return 0;
    }

    if (offset + size > files[file_index].size) {
        size = files[file_index].size - offset;
    }

    // Читаем данные из архива
    int fd = open(archive_path, O_RDONLY);
    if (fd == -1) {
        log_op("ARCHIVE_READ", path, -errno);
        return -errno;
    }

    off_t read_offset = files[file_index].offset + offset;
    int res = pread(fd, buf, size, read_offset);
    
    if (res == -1) {
        res = -errno;
    }

    close(fd);
    log_op("ARCHIVE_READ", path, res);
    return res;
}

// Запрещенные операции (read-only)
static int archive_write(const char *path, const char *buf, size_t size, off_t offset,
                        struct fuse_file_info *fi) {
    (void)buf; (void)size; (void)offset; (void)fi;
    if (!path) return -EINVAL;
    log_op("ARCHIVE_WRITE", path, -EROFS);
    return -EROFS;
}

static int archive_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void)mode; (void)fi;
    if (!path) return -EINVAL;
    log_op("ARCHIVE_CREATE", path, -EROFS);
    return -EROFS;
}

static int archive_unlink(const char *path) {
    if (!path) return -EINVAL;
    log_op("ARCHIVE_UNLINK", path, -EROFS);
    return -EROFS;
}

static int archive_mkdir(const char *path, mode_t mode) {
    (void)mode;
    if (!path) return -EINVAL;
    log_op("ARCHIVE_MKDIR", path, -EROFS);
    return -EROFS;
}

static int archive_rmdir(const char *path) {
    if (!path) return -EINVAL;
    log_op("ARCHIVE_RMDIR", path, -EROFS);
    return -EROFS;
}

static struct fuse_operations archive_ops = {
    .getattr = archive_getattr,
    .readdir = archive_readdir,
    .open = archive_open,
    .read = archive_read,
    .write = archive_write,
    .create = archive_create,
    .unlink = archive_unlink,
    .mkdir = archive_mkdir,
    .rmdir = archive_rmdir,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <archive_file> <mount_point> [FUSE options]\n", argv[0]);
        fprintf(stderr, "Example: %s /path/to/archive.tar /mnt/archive -f\n", argv[0]);
        return 1;
    }

    // Сохраняем путь к архиву
    archive_path = realpath(argv[1], NULL);
    if (!archive_path) {
        fprintf(stderr, "Error: Cannot resolve archive path %s\n", argv[1]);
        return 1;
    }

    // Парсим архив
    if (parse_tar_archive(archive_path) != 0) {
        fprintf(stderr, "Error: Failed to parse archive %s\n", archive_path);
        free(archive_path);
        return 1;
    }

    fprintf(stderr, "Archive FS: Mounting '%s' at '%s'\n", archive_path, argv[2]);
    fprintf(stderr, "Found %d files in archive\n", file_count);
    fprintf(stderr, "All files are read-only.\n");

    // Подготавливаем аргументы для FUSE
    argv[1] = argv[2];
    int ret = fuse_main(argc - 1, argv + 1, &archive_ops, NULL);

    free(archive_path);
    return ret;
}
