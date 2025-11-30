#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>

struct tar_entry {
    char name[256];
    size_t size;
    off_t data_offset;
    int is_dir;
    struct tar_entry *next;
};

static char *archive_path = NULL;
static int archive_fd = -1;
static struct tar_entry *entries_head = NULL;

static long octal_to_decimal(const char *octal) {
    return strtol(octal, NULL, 8);
}

static void parse_tar_archive() {
    archive_fd = open(archive_path, O_RDONLY);
    if (archive_fd < 0) {
        perror("Cannot open archive");
        exit(1);
    }

    char block[512];
    off_t current_offset = 0;

    while (1) {
        ssize_t res = read(archive_fd, block, 512);
        if (res < 512) break;
        if (block[0] == 0) break;

        struct tar_entry *entry = malloc(sizeof(struct tar_entry));
        strncpy(entry->name, block, 100);
        entry->name[100] = '\0';
        
        char size_str[12] = {0};
        memcpy(size_str, block + 124, 11);
        entry->size = octal_to_decimal(size_str);
        
        char type = block[156];
        entry->is_dir = (type == '5' || entry->name[strlen(entry->name) - 1] == '/');

        if (entry->is_dir && entry->name[strlen(entry->name) - 1] == '/') {
            entry->name[strlen(entry->name) - 1] = '\0';
        }

        entry->data_offset = current_offset + 512;
        entry->next = entries_head;
        entries_head = entry;

        size_t skip = (entry->size + 511) / 512 * 512;
        lseek(archive_fd, skip, SEEK_CUR);
        current_offset += 512 + skip;
    }
}

static struct tar_entry *find_entry(const char *path) {
    const char *target = path;
    if (target[0] == '/') target++;
    if (strlen(target) == 0) return NULL;

    struct tar_entry *curr = entries_head;
    while (curr) {
        if (strcmp(curr->name, target) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

static int do_getattr(const char *path, struct stat *st) {
    memset(st, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        st->st_mode = S_IFDIR | 0555;
        st->st_nlink = 2;
        return 0;
    }

    struct tar_entry *entry = find_entry(path);
    if (entry) {
        if (entry->is_dir) {
            st->st_mode = S_IFDIR | 0555;
            st->st_nlink = 2;
        } else {
            st->st_mode = S_IFREG | 0444;
            st->st_nlink = 1;
            st->st_size = entry->size;
        }
        return 0;
    }
    return -ENOENT;
}

static int do_readdir(const char *path, void *buffer, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    filler(buffer, ".", NULL, 0, 0);
    filler(buffer, "..", NULL, 0, 0);

    const char *dir_path = path;
    if (dir_path[0] == '/') dir_path++;
    size_t dir_len = strlen(dir_path);

    struct tar_entry *curr = entries_head;
    while (curr) {
        if (dir_len == 0) {
            if (strchr(curr->name, '/') == NULL) 
                filler(buffer, curr->name, NULL, 0, 0);
        } else {
            if (strncmp(curr->name, dir_path, dir_len) == 0 && curr->name[dir_len] == '/') {
                char *sub = curr->name + dir_len + 1;
                if (strchr(sub, '/') == NULL && strlen(sub) > 0)
                    filler(buffer, sub, NULL, 0, 0);
            }
        }
        curr = curr->next;
    }
    return 0;
}

static int do_open(const char *path, struct fuse_file_info *fi) {
    if (find_entry(path) != NULL) return 0;
    return -ENOENT;
}

static int do_read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
    struct tar_entry *entry = find_entry(path);
    if (!entry || entry->is_dir) return -EISDIR;
    
    if (offset >= entry->size) return 0;
    if (offset + size > entry->size) size = entry->size - offset;

    ssize_t res = pread(archive_fd, buf, size, entry->data_offset + offset);
    if (res == -1) return -errno;
    return res;
}

static struct fuse_operations operations = {
    .getattr = do_getattr,
    .readdir = do_readdir,
    .open    = do_open,
    .read    = do_read,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <archive.tar> <mount_point>\n", argv[0]);
        return 1;
    }
    archive_path = realpath(argv[1], NULL);
    parse_tar_archive();
    char *fuse_argv[] = { argv[0], argv[2], "-f", NULL };
    return fuse_main(3, fuse_argv, &operations, NULL);
}
