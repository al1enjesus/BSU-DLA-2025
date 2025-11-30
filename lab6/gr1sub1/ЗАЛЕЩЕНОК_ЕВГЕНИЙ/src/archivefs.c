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

#define TAR_BLOCK_SIZE 512
#define TAR_NAME_LEN 100
#define TAR_SIZE_OFFSET 124
#define TAR_TYPE_OFFSET 156

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

static void free_entries() {
    struct tar_entry *curr = entries_head;
    while (curr) {
        struct tar_entry *next = curr->next;
        free(curr);
        curr = next;
    }
    if (archive_fd >= 0) {
        close(archive_fd);
    }
}

static long octal_to_decimal(const char *octal) {
    return strtol(octal, NULL, 8);
}

static void parse_tar_archive() {
    archive_fd = open(archive_path, O_RDONLY);
    if (archive_fd < 0) {
        perror("Cannot open archive");
        exit(1);
    }

    char block[TAR_BLOCK_SIZE];
    off_t current_offset = 0;

    while (1) {
        ssize_t res = read(archive_fd, block, TAR_BLOCK_SIZE);
        if (res < TAR_BLOCK_SIZE) break; 
        
        if (block[0] == 0) break; 

        struct tar_entry *entry = malloc(sizeof(struct tar_entry));
        if (!entry) {
            perror("Memory allocation failed");
            free_entries();
            exit(1);
        }

        snprintf(entry->name, sizeof(entry->name), "%.*s", TAR_NAME_LEN, block);
        
        char size_str[12] = {0};
        memcpy(size_str, block + TAR_SIZE_OFFSET, 11);
        entry->size = octal_to_decimal(size_str);
        
        char type = block[TAR_TYPE_OFFSET];
        size_t len = strlen(entry->name);
        
        entry->is_dir = (type == '5' || (len > 0 && entry->name[len - 1] == '/'));

        if (entry->is_dir && len > 0 && entry->name[len - 1] == '/') {
            entry->name[len - 1] = '\0';
        }

        entry->data_offset = current_offset + TAR_BLOCK_SIZE;
        entry->next = entries_head;
        entries_head = entry;

        size_t skip = (entry->size + TAR_BLOCK_SIZE - 1) / TAR_BLOCK_SIZE * TAR_BLOCK_SIZE;
        
        if (lseek(archive_fd, skip, SEEK_CUR) == (off_t)-1) {
             perror("Lseek failed inside archive");
             free_entries();
             exit(1);
        }
        current_offset += TAR_BLOCK_SIZE + skip;
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
    if (!archive_path) {
        perror("Error resolving archive path");
        return 1;
    }
    
    atexit(free_entries);
    
    parse_tar_archive();
    
    char *fuse_argv[] = { argv[0], argv[2], "-f", NULL };
    return fuse_main(3, fuse_argv, &operations, NULL);
}
