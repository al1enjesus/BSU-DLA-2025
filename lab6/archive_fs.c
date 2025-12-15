#define _GNU_SOURCE
#define FUSE_USE_VERSION 35
#include "archive_fs.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

struct tar_file_entry {
    char name[256];
    size_t size;
    off_t offset;
};

static struct tar_file_entry entries[1024];
static int entry_count = 0;

static const char *tar_file;

/* структура tar header (512 байт) */
struct posix_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
};

static void parse_tar() {
    int fd = open(tar_file, O_RDONLY);
    if (fd < 0) return;

    off_t offset = 0;

    while (1) {
        struct posix_header hdr;
        int r = read(fd, &hdr, 512);
        if (r != 512) break;

        if (hdr.name[0] == '\0')
            break;

        size_t size = strtol(hdr.size, NULL, 8);

        strncpy(entries[entry_count].name, hdr.name, 255);
        entries[entry_count].size = size;
        entries[entry_count].offset = offset + 512;
        entry_count++;

        off_t skip = ((size + 511) / 512) * 512;
        lseek(fd, skip, SEEK_CUR);
        offset += 512 + skip;
    }

    close(fd);
}

/* ---------------- getattr ---------------- */
static int ar_getattr(const char *path, struct stat *st, struct fuse_file_info *fi)
{
    (void)fi;
    memset(st, 0, sizeof(*st));

    if (strcmp(path, "/") == 0) {
        st->st_mode = S_IFDIR | 0555;
        st->st_nlink = 2;
        return 0;
    }

    const char *p = path + 1;

    for (int i = 0; i < entry_count; i++) {
        if (strcmp(entries[i].name, p) == 0) {
            st->st_mode = S_IFREG | 0444;
            st->st_size = entries[i].size;
            st->st_nlink = 1;
            return 0;
        }
    }

    return -ENOENT;
}

/* ---------------- readdir ---------------- */
static int ar_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t off, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags)
{
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    for (int i = 0; i < entry_count; i++) {
        filler(buf, entries[i].name, NULL, 0, 0);
    }

    return 0;
}

/* ---------------- open ---------------- */
static int ar_open(const char *path, struct fuse_file_info *fi) {
    fi->flags &= ~O_WRONLY;
    fi->flags &= ~O_RDWR;
    return 0;
}

/* ---------------- read ---------------- */
static int ar_read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi)
{
    const char *p = path + 1;
    int idx = -1;

    for (int i = 0; i < entry_count; i++)
        if (strcmp(entries[i].name, p) == 0)
            idx = i;

    if (idx == -1)
        return -ENOENT;

    int fd = open(tar_file, O_RDONLY);
    if (fd < 0) return -errno;

    if (offset >= entries[idx].size) {
        close(fd);
        return 0;
    }

    if (offset + size > entries[idx].size)
        size = entries[idx].size - offset;

    lseek(fd, entries[idx].offset + offset, SEEK_SET);
    int r = read(fd, buf, size);
    close(fd);

    return r;
}

static struct fuse_operations ar_ops;

struct fuse_operations *get_archive_ops(const char *path) {
    tar_file = path;
    parse_tar();

    memset(&ar_ops, 0, sizeof(ar_ops));
    ar_ops.getattr = ar_getattr;
    ar_ops.readdir = ar_readdir;
    ar_ops.open    = ar_open;
    ar_ops.read    = ar_read;

    return &ar_ops;
}
