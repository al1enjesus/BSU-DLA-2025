#ifndef ARCHIVE_OPS_H
#define ARCHIVE_OPS_H

#include <fuse3/fuse.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

struct archive_file {
    char *path;            // Путь файла (относительный, без ведущего /)
    off_t size;            // Размер файла
    off_t offset;          // Смещение в архиве
    mode_t mode;           // Режим доступа
    time_t mtime;          // Время модификации
    uid_t uid;             // Владелец
    gid_t gid;             // Группа
    int is_dir;            // Это директория?
    struct archive_file *next;  // Следующий файл в списке
};

struct archive_data {
    char *archive_path;    // Путь к архиву
    off_t archive_size;    // Размер архива
    int fd;                // Файловый дескриптор архива
    struct archive_file *files;  // Список файлов
    int file_count;        // Количество файлов
    int dir_count;         // Количество директорий
    pthread_mutex_t mutex; // Мьютекс для потокобезопасности
};

// Безопасная нормализация пути
char* safe_normalize_path(const char *path);

// Инициализация и завершение
void* archive_init(struct fuse_conn_info *conn, struct fuse_config *cfg);
void archive_destroy(void *private_data);

// Операции FUSE
int archive_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi);
int archive_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                    off_t offset, struct fuse_file_info *fi,
                    enum fuse_readdir_flags flags);
int archive_open(const char *path, struct fuse_file_info *fi);
int archive_read(const char *path, char *buf, size_t size, off_t offset,
                 struct fuse_file_info *fi);
int archive_statfs(const char *path, struct statvfs *stbuf);
int archive_access(const char *path, int mask);

// Внутренние функции
struct archive_file* find_file(struct archive_data *data, const char *path);
int parse_tar_archive(struct archive_data *data);

#endif
