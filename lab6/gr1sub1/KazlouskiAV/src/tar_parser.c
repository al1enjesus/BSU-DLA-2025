#include "../include/fuse_fs.h"

tar_archive_t g_tar = {0};

// Простой парсинг tar (без поддержки длинных имён, только базовый формат)
static unsigned long octal_to_ulong(const char *buf, size_t len) {
    unsigned long val = 0;
    for (size_t i = 0; i < len && buf[i] >= '0' && buf[i] <= '7'; i++) {
        val = val * 8 + (buf[i] - '0');
    }
    return val;
}

int tar_load(const char *tar_path) {
    int fd = open(tar_path, O_RDONLY);
    if (fd == -1) return -1;

    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        return -1;
    }

    g_tar.size = st.st_size;
    g_tar.data = mmap(NULL, g_tar.size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (g_tar.data == MAP_FAILED) {
        g_tar.data = NULL;
        return -1;
    }

    size_t offset = 0;
    while (offset + 512 <= g_tar.size) {
        char *header = g_tar.data + offset;
        if (header[0] == '\0') break; // конец архива

        char name[101];
        strncpy(name, header, 100);
        name[100] = '\0';

        unsigned long size = octal_to_ulong(header + 124, 12);
        unsigned long mode = octal_to_ulong(header + 100, 8);
        unsigned long mtime = octal_to_ulong(header + 136, 12);

        if (name[strlen(name)-1] == '/') {
            offset += 512 + ((size + 511) / 512) * 512;
            continue;
        }

        g_tar.entry_count++;
        offset += 512 + ((size + 511) / 512) * 512;
    }

    g_tar.entries = calloc(g_tar.entry_count, sizeof(tar_entry_t));
    if (!g_tar.entries) {
        munmap(g_tar.data, g_tar.size);
        g_tar.data = NULL;
        return -1;
    }

    offset = 0;
    size_t idx = 0;
    while (offset + 512 <= g_tar.size && idx < g_tar.entry_count) {
        char *header = g_tar.data + offset;
        if (header[0] == '\0') break;

        char name[101];
        strncpy(name, header, 100);
        name[100] = '\0';

        unsigned long size = octal_to_ulong(header + 124, 12);
        unsigned long mode = octal_to_ulong(header + 100, 8);
        unsigned long mtime = octal_to_ulong(header + 136, 12);

        if (name[strlen(name)-1] == '/') {
            offset += 512 + ((size + 511) / 512) * 512;
            continue;
        }

        strncpy(g_tar.entries[idx].path, name, 255);
        g_tar.entries[idx].path[255] = '\0';
        g_tar.entries[idx].offset = offset + 512;
        g_tar.entries[idx].size = size;
        g_tar.entries[idx].mode = mode;
        g_tar.entries[idx].mtime = mtime;
        idx++;

        offset += 512 + ((size + 511) / 512) * 512;
    }

    return 0;
}

void tar_free(tar_archive_t *tar) {
    if (tar->data) {
        munmap(tar->data, tar->size);
        tar->data = NULL;
    }
    free(tar->entries);
    tar->entries = NULL;
    tar->entry_count = 0;
    tar->size = 0;
}

tar_entry_t *tar_find(const tar_archive_t *tar, const char *path) {
    if (path[0] == '/') path++;
    for (size_t i = 0; i < tar->entry_count; i++) {
        if (strcmp(tar->entries[i].path, path) == 0) {
            return (tar_entry_t *)&tar->entries[i];
        }
    }
    return NULL;
}

int tar_list_root(const tar_archive_t *tar, void *buf, fuse_fill_dir_t filler) {
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    // Просто все файлы в корне (без поддиректорий)
    for (size_t i = 0; i < tar->entry_count; i++) {
        // Извлекаем имя файла без пути
        const char *slash = strrchr(tar->entries[i].path, '/');
        const char *name = slash ? slash + 1 : tar->entries[i].path;
        filler(buf, name, NULL, 0, 0);
    }
    return 0;
}