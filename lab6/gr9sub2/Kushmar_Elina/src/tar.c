#include "tar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct tar_entry entries[1024];
static int entry_count = 0;
static FILE *tar_file;

static size_t oct2dec(const char *oct, size_t size) {
    size_t n = 0;
    for (size_t i = 0; i < size && oct[i]; i++)
        n = n * 8 + (oct[i] - '0');
    return n;
}

int tar_load(const char *path) {
    tar_file = fopen(path, "rb");
    if (!tar_file) return -1;

    entry_count = 0;
    while (1) {
        char block[TAR_BLOCK];
        if (fread(block, 1, TAR_BLOCK, tar_file) < TAR_BLOCK)
            break;

        if (block[0] == '\0')
            break;

        struct tar_entry *e = &entries[entry_count];
        strncpy(e->name, block, 100);
        e->size = oct2dec(block + 124, 12);
        e->is_dir = (block[156] == '5');
        e->offset = ftell(tar_file);

        entry_count++;

        size_t skip = ((e->size + 511) / 512) * 512;
        fseek(tar_file, skip, SEEK_CUR);
    }
    return 0;
}

struct tar_entry* tar_find(const char *path) {
    if (path[0] == '/') path++;
    for (int i = 0; i < entry_count; i++)
        if (strcmp(path, entries[i].name) == 0)
            return &entries[i];
    return NULL;
}

int tar_list(const char *path, void *buf, fuse_fill_dir_t filler) {
    if (strcmp(path, "/") == 0) {
        filler(buf, ".", NULL, 0, 0);
        filler(buf, "..", NULL, 0, 0);

        for (int i = 0; i < entry_count; i++) {
            const char *name = entries[i].name;
            if (strchr(name, '/') == NULL)
                filler(buf, name, NULL, 0, 0);
        }
    }
    return 0;
}

int tar_read(const char *path, char *buf, size_t size, off_t offset) {
    struct tar_entry *e = tar_find(path);
    if (!e || e->is_dir) return -1;

    fseek(tar_file, e->offset + offset, SEEK_SET);
    return fread(buf, 1, size, tar_file);
}
