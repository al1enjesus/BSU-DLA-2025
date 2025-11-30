#include "tar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct tar_entry entries[1024];
static int entry_count = 0;
static FILE *tar_file = NULL;

static size_t oct2dec(const char *oct, size_t size) {
    size_t n = 0;
    for (size_t i = 0; i < size && oct[i]; i++)
        n = n * 8 + (oct[i] - '0');
    return n;
}

int tar_load(const char *path) {
    tar_file = fopen(path, "rb");
    if (!tar_file) {
        perror("fopen");
        return -1;
    }

    entry_count = 0;
    while (entry_count < 1024) {
        char block[TAR_BLOCK];
        if (fread(block, 1, TAR_BLOCK, tar_file) < TAR_BLOCK)
            break;

        if (block[0] == '\0')
            break;

        struct tar_entry *e = &entries[entry_count];
        strncpy(e->name, block, 99);
        e->name[99] = '\0';
        e->size = oct2dec(block + 124, 12);
        e->is_dir = (block[156] == '5');
        e->offset = ftell(tar_file);

        entry_count++;

        size_t skip = ((e->size + 511) / 512) * 512;
        if (fseek(tar_file, skip, SEEK_CUR) != 0) {
            break;
        }
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
        for (int i = 0; i < entry_count; i++) {
            const char *name = entries[i].name;
            char *slash = strchr(name, '/');
            if (slash == NULL || slash == name || *(slash + 1) == '\0') {
                const char *display_name = name;
                if (slash != NULL && slash == name) {
                    display_name = name + 1;
                }
                filler(buf, display_name, NULL, 0, 0);
            }
        }
    } else {
        const char *search_path = path + 1;
        size_t search_len = strlen(search_path);
        
        for (int i = 0; i < entry_count; i++) {
            if (strncmp(entries[i].name, search_path, search_len) == 0) {
                const char *remaining = entries[i].name + search_len;
                if (remaining[0] == '/' && strchr(remaining + 1, '/') == NULL) {
                    filler(buf, remaining + 1, NULL, 0, 0);
                }
            }
        }
    }
    return 0;
}

int tar_read(const char *path, char *buf, size_t size, off_t offset) {
    struct tar_entry *e = tar_find(path);
    if (!e || e->is_dir) return -1;
    
    if (offset >= e->size) return 0;
    if (offset + size > e->size) size = e->size - offset;

    if (fseek(tar_file, e->offset + offset, SEEK_SET) != 0) {
        return -1;
    }
    
    int res = fread(buf, 1, size, tar_file);
    if (ferror(tar_file)) {
        return -1;
    }
    return res;
}