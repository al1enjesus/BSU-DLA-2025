#ifndef TAR_H
#define TAR_H

#include <stddef.h>

#define TAR_BLOCK 512

struct tar_entry {
    char name[100];
    size_t size;
    size_t offset;
    int is_dir;
};

int tar_load(const char *path);
struct tar_entry *tar_find(const char *path);
int tar_list(const char *path, void *buf, fuse_fill_dir_t filler);
int tar_read(const char *path, char *buf, size_t size, off_t offset);

#endif
