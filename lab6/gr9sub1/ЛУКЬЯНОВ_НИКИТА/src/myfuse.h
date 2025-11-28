#ifndef MYFUSE_H
#define MYFUSE_H

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
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <ctype.h>

#define MY_MAX_PATH 1024

// Структура для хранения состояния файловой системы
struct fs_state {
    char *rootdir;
};

// Макрос для получения доступа к private_data
#define FS_DATA ((struct fs_state *) fuse_get_context()->private_data)

// Получение полного пути к реальному файлу
void get_full_path(char *fpath, const char *path);

// Логирование операций с временной меткой
void log_msg(const char *op, const char *path, int ret);

// Получение атрибутов файла (ls -l)
int do_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi);

// Чтение директории (ls)
int do_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags);

// Создание директории (mkdir)
int do_mkdir(const char *path, mode_t mode);

// Удаление директории (rmdir)
int do_rmdir(const char *path);

// Удаление файла (unlink)
int do_unlink(const char *path);

// Создание файла (touch / creat)
int do_create(const char *path, mode_t mode, struct fuse_file_info *fi);

// Открытие файла (cat / open)
int do_open(const char *path, struct fuse_file_info *fi);

//чтение файла
int do_read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi);

//запись в файл
int do_write(const char *path, const char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi);

// Регистрация операций
extern struct fuse_operations operations;

int myfuse_main(int argc, char *argv[]);

#endif
