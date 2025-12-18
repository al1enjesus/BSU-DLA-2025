#ifndef OPERATIONS_H
#define OPERATIONS_H

#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <sys/stat.h>
#include <signal.h>  // Добавлено для sig_atomic_t

/* Глобальные переменные */
extern char *base_path;
extern volatile sig_atomic_t stop_flag;

/* Вспомогательные функции */
char* get_full_path(const char *path);
void log_operation(const char *op, const char *path, int result);
void log_operation_bytes(const char *op, const char *path, size_t bytes, off_t offset, int result);
int check_path_safety(const char *path);  // Добавлено
void signal_handler(int sig);  // Добавлено

/* Основные операции FUSE */
int passthrough_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi);
int passthrough_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags);
int passthrough_open(const char *path, struct fuse_file_info *fi);
int passthrough_read(const char *path, char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi);
int passthrough_write(const char *path, const char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi);
int passthrough_create(const char *path, mode_t mode, struct fuse_file_info *fi);
int passthrough_unlink(const char *path);
int passthrough_mkdir(const char *path, mode_t mode);
int passthrough_rmdir(const char *path);
int passthrough_rename(const char *oldpath, const char *newpath, unsigned int flags);
int passthrough_chmod(const char *path, mode_t mode, struct fuse_file_info *fi);
int passthrough_chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi);
int passthrough_truncate(const char *path, off_t size, struct fuse_file_info *fi);
int passthrough_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi);
int passthrough_statfs(const char *path, struct statvfs *stbuf);

/* Операции для работы с файловыми дескрипторами */
int passthrough_release(const char *path, struct fuse_file_info *fi);
int passthrough_flush(const char *path, struct fuse_file_info *fi);
int passthrough_fsync(const char *path, int isdatasync, struct fuse_file_info *fi);

#endif /* OPERATIONS_H */
