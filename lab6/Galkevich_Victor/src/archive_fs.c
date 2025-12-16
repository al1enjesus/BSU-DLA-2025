#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>

/* Описание одного элемента из tar */
struct tar_entry {
    char *path;            /* путь вида "/dir/file.txt" */
    off_t data_offset;     /* смещение данных файла внутри tar */
    size_t size;           /* размер файла */
    mode_t mode;           /* права из заголовка */
    int is_dir;            /* 1 = директория, 0 = файл */
    struct tar_entry *next;
};

static struct tar_entry *g_entries = NULL;
static int g_tar_fd = -1;

/* Проверка, весь ли 512-байтовый блок из нулей (конец архива) */
static int is_zero_block(const unsigned char *block)
{
    for (int i = 0; i < 512; i++) {
        if (block[i] != 0)
            return 0;
    }
    return 1;
}

/* Cклеить prefix + name и привести к виду "/path/without/trailing/slash" */
static char *make_canonical_path(const char *name, const char *prefix)
{
    char temp[256];

    temp[0] = '\0';

    if (prefix && prefix[0] != '\0') {
        snprintf(temp, sizeof(temp), "%s/%s", prefix, name);
    } else {
        snprintf(temp, sizeof(temp), "%s", name);
    }

    /* Обрезаем завершающие '/' */
    size_t len = strlen(temp);
    while (len > 0 && temp[len - 1] == '/') {
        temp[--len] = '\0';
    }

    if (len == 0)
        return NULL;    /* пустое имя — пропускаем */

    char *res = (char *)malloc(len + 2);
    if (!res)
        return NULL;

    res[0] = '/';
    memcpy(res + 1, temp, len);
    res[len + 1] = '\0';
    return res;
}

/* Добавить запись в список */
static void add_entry(struct tar_entry *e)
{
    e->next = g_entries;
    g_entries = e;
}

/* Поиск записи по пути */
static struct tar_entry *find_entry(const char *path)
{
    struct tar_entry *cur = g_entries;
    while (cur) {
        if (strcmp(cur->path, path) == 0)
            return cur;
        cur = cur->next;
    }
    return NULL;
}

/* Проверка, является ли путь директорией (явной или неявной) */
static int path_is_dir(const char *path)
{
    if (strcmp(path, "/") == 0)
        return 1;

    struct tar_entry *e = find_entry(path);
    if (e && e->is_dir)
        return 1;

    size_t len = strlen(path);
    struct tar_entry *cur = g_entries;
    while (cur) {
        if (strncmp(cur->path, path, len) == 0 && cur->path[len] == '/')
            return 1;
        cur = cur->next;
    }

    return 0;
}

/* Загрузка и парсинг tar-файла */
static int load_tar(const char *tar_path)
{
    g_tar_fd = open(tar_path, O_RDONLY);
    if (g_tar_fd < 0) {
        perror("open tar");
        return -1;
    }

    off_t offset = 0;

    while (1) {
        unsigned char header[512];
        ssize_t r = pread(g_tar_fd, header, 512, offset);
        if (r == 0) {
            /* конец файла */
            break;
        }
        if (r < 0) {
            perror("pread");
            return -1;
        }
        if (r != 512) {
            fprintf(stderr, "Short read on tar header\n");
            return -1;
        }

        if (is_zero_block(header)) {
            /* конец архива */
            break;
        }

        char name[101];
        char prefix[156];
        char sizebuf[13];
        char modebuf[9];
        char typeflag;

        memcpy(name, header + 0, 100);
        name[100] = '\0';
        memcpy(modebuf, header + 100, 8);
        modebuf[8] = '\0';
        memcpy(sizebuf, header + 124, 12);
        sizebuf[12] = '\0';
        memcpy(prefix, header + 345, 155);
        prefix[155] = '\0';
        typeflag = header[156];

        size_t size = strtoull(sizebuf, NULL, 8);
        mode_t mode = (mode_t)strtol(modebuf, NULL, 8);

        char *fullpath = make_canonical_path(name, prefix);
        if (fullpath) {
            struct tar_entry *e =
                (struct tar_entry *)malloc(sizeof(struct tar_entry));
            if (!e) {
                free(fullpath);
                return -1;
            }

            e->path = fullpath;
            e->size = size;
            e->mode = mode;
            e->is_dir = (typeflag == '5');
            e->data_offset = offset + 512;
            add_entry(e);
        }

        /* Переходим к следующему заголовку: размер выровнен на 512 */
        off_t data_size = ((size + 511) / 512) * 512;
        offset += 512 + data_size;
    }

    return 0;
}

static void free_entries(void)
{
    struct tar_entry *cur = g_entries;
    while (cur) {
        struct tar_entry *next = cur->next;
        free(cur->path);
        free(cur);
        cur = next;
    }
    g_entries = NULL;
}

/* getattr */
static int tarfs_getattr(const char *path, struct stat *stbuf)
{
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0555;
        stbuf->st_nlink = 2;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        return 0;
    }

    struct tar_entry *e = find_entry(path);
    if (e) {
        if (e->is_dir) {
            stbuf->st_mode = S_IFDIR | 0555;
            stbuf->st_nlink = 2;
        } else {
            stbuf->st_mode = S_IFREG | 0444;
            stbuf->st_nlink = 1;
            stbuf->st_size = e->size;
        }
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        return 0;
    }

    /* Неявная директория (есть файлы с таким префиксом) */
    if (path_is_dir(path)) {
        stbuf->st_mode = S_IFDIR | 0555;
        stbuf->st_nlink = 2;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        return 0;
    }

    return -ENOENT;
}

/* readdir */
static int tarfs_readdir(const char *path, void *buf,
                         fuse_fill_dir_t filler, off_t offset,
                         struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;

    if (!path_is_dir(path))
        return -ENOENT;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    size_t dirlen = strlen(path);

    /* список уже добавленных имён в этой директории, чтобы не дублировать */
    struct child_name {
        char *name;
        struct child_name *next;
    };
    struct child_name *list = NULL;

    struct tar_entry *e = g_entries;
    while (e) {
        const char *p = e->path;

        if (strcmp(path, "/") == 0) {
            /* Для корня: берём первую компоненту пути после '/' */
            if (p[0] != '/'){
                e = e->next;
                continue;
            }
            const char *rest = p + 1;
            if (*rest == '\0') {
                e = e->next;
                continue; /* сам корень, пропускаем */
            }

            const char *slash = strchr(rest, '/');
            size_t name_len = slash ? (size_t)(slash - rest) : strlen(rest);
            if (name_len == 0 || name_len >= PATH_MAX) {
                e = e->next;
                continue;
            }

            char name[PATH_MAX];
            memcpy(name, rest, name_len);
            name[name_len] = '\0';

            struct child_name *c = list;
            int found = 0;
            while (c) {
                if (strcmp(c->name, name) == 0) {
                    found = 1;
                    break;
                }
                c = c->next;
            }
            if (!found) {
                struct child_name *nc =
                    (struct child_name *)malloc(sizeof(struct child_name));
                nc->name = strdup(name);
                nc->next = list;
                list = nc;
                filler(buf, nc->name, NULL, 0);
            }
        } else {
            if (strncmp(p, path, dirlen) != 0) {
                e = e->next;
                continue;
            }
            const char *rest = p + dirlen;
            if (rest[0] == '\0') {
                e = e->next;
                continue; /* сам каталог */
            }
            if (rest[0] != '/') {
                e = e->next;
                continue;
            }
            rest++; /* пропускаем '/' */
            if (rest[0] == '\0') {
                e = e->next;
                continue;
            }

            const char *slash = strchr(rest, '/');
            size_t name_len = slash ? (size_t)(slash - rest) : strlen(rest);
            if (name_len == 0 || name_len >= PATH_MAX) {
                e = e->next;
                continue;
            }

            char name[PATH_MAX];
            memcpy(name, rest, name_len);
            name[name_len] = '\0';

            struct child_name *c = list;
            int found = 0;
            while (c) {
                if (strcmp(c->name, name) == 0) {
                    found = 1;
                    break;
                }
                c = c->next;
            }
            if (!found) {
                struct child_name *nc =
                    (struct child_name *)malloc(sizeof(struct child_name));
                nc->name = strdup(name);
                nc->next = list;
                list = nc;
                filler(buf, nc->name, NULL, 0);
            }
        }

        e = e->next;
    }

    /* освобождаем список временных имён */
    struct child_name *c = list;
    while (c) {
        struct child_name *next = c->next;
        free(c->name);
        free(c);
        c = next;
    }

    return 0;
}

/* open: только для чтения */
static int tarfs_open(const char *path, struct fuse_file_info *fi)
{
    if ((fi->flags & O_ACCMODE) != O_RDONLY)
        return -EACCES;

    struct tar_entry *e = find_entry(path);
    if (!e)
        return -ENOENT;
    if (e->is_dir)
        return -EISDIR;

    /* сохраняем указатель на запись в fh */
    fi->fh = (uintptr_t)e;
    return 0;
}

/* read */
static int tarfs_read(const char *path, char *buf, size_t size,
                      off_t offset, struct fuse_file_info *fi)
{
    (void)path;
    struct tar_entry *e = NULL;

    if (fi && fi->fh)
        e = (struct tar_entry *)(uintptr_t)fi->fh;
    else
        e = find_entry(path);

    if (!e)
        return -ENOENT;
    if (e->is_dir)
        return -EISDIR;

    if ((off_t)offset >= (off_t)e->size)
        return 0;

    if (offset + (off_t)size > (off_t)e->size)
        size = e->size - offset;

    ssize_t res = pread(g_tar_fd, buf, size, e->data_offset + offset);
    if (res < 0)
        return -errno;

    return (int)res;
}

static struct fuse_operations tarfs_oper = {
    .getattr = tarfs_getattr,
    .readdir = tarfs_readdir,
    .open    = tarfs_open,
    .read    = tarfs_read,
};

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr,
                "Usage: %s <archive.tar> <mountpoint> [FUSE options]\n",
                argv[0]);
        return 1;
    }

    const char *tar_path = argv[1];
    if (load_tar(tar_path) != 0) {
        fprintf(stderr, "Failed to load tar archive\n");
        return 1;
    }

    /* убираем путь к tar из аргументов fuse */
    int fuse_argc = argc - 1;
    char **fuse_argv = (char **)malloc(sizeof(char *) * fuse_argc);
    if (!fuse_argv) {
        perror("malloc");
        close(g_tar_fd);
        free_entries();
        return 1;
    }

    fuse_argv[0] = argv[0];
    for (int i = 2; i < argc; i++) {
        fuse_argv[i - 1] = argv[i];
    }

    int ret = fuse_main(fuse_argc, fuse_argv, &tarfs_oper, NULL);

    free(fuse_argv);
    if (g_tar_fd != -1)
        close(g_tar_fd);
    free_entries();

    return ret;
}