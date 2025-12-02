#ifndef OPERATIONS_H
#define OPERATIONS_H

#include <fuse3/fuse.h>
#include <sys/stat.h>

#define PATH_MAX_LEN 4096

enum fs_mode {
    MODE_PASSTHROUGH = 0,
    MODE_ROT13,
    MODE_UPPERCASE,
};

/* Конфигурация файловой системы */
struct fs_config {
    /* Корневая (исходная) директория, которую мы зеркалируем */
    char root[PATH_MAX_LEN];
    /* Режим работы файловой системы: passthrough, rot13, uppercase */
    enum fs_mode mode;
};

/* Глобальная конфигурация, используемая в реализации операций */
extern struct fs_config g_config;

/* FUSE callbacks (функции-обработчики операций). Все функции возвращают 0
   при успехе или отрицательное значение ошибки (например, -ENOENT, -EACCES).

   Параметры:
   - path: путь внутри смонтированной FUSE FS (начинается с '/').
   - stbuf: буфер для заполнения метаданных (struct stat).
   - fi: структура с информацией о открытом файле (fuse_file_info).

   Комментарии к каждой функции находятся в реализациях (operations.c).
*/
int fs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi);
int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags);
int fs_open(const char *path, struct fuse_file_info *fi);
int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi);
int fs_unlink(const char *path);
int fs_mkdir(const char *path, mode_t mode);
int fs_rmdir(const char *path);
int fs_release(const char *path, struct fuse_file_info *fi);
int fs_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi);
int fs_access(const char *path, int mask);
int fs_flush(const char *path, struct fuse_file_info *fi);

#endif // OPERATIONS_H
