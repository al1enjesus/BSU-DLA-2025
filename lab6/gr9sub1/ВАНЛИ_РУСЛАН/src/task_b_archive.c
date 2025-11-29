#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

// Структура для хранения метаданных файла внутри TAR
struct tar_entry {
    char path[256];      // Полный путь внутри архива
    size_t size;         // Размер файла
    off_t data_offset;   // Смещение данных в .tar файле
    int is_dir;          // 1 если директория
    struct tar_entry *next;
};

static struct tar_entry *head = NULL;
static char *tar_path = NULL; // Путь к самому .tar файлу
static int tar_fd = -1;       // Дескриптор открытого .tar файла

// --- Парсер TAR (ustar format) ---
// Стандартный блок TAR = 512 байт
struct posix_header {
    char name[100]; char mode[8]; char uid[8]; char gid[8];
    char size[12]; char mtime[12]; char chksum[8]; char typeflag;
    char linkname[100]; char magic[6]; char version[2];
    char uname[32]; char gname[32]; char devmajor[8]; char devminor[8];
    char prefix[155];
};

// Чтение заголовка и добавление в список
void parse_tar(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) { perror("Failed to open tar"); exit(1); }

    char block[512];
    off_t current_offset = 0;

    while (fread(block, 1, 512, f) == 512) {
        if (block[0] == 0) break; // Пустой блок = конец архива

        struct posix_header *h = (struct posix_header *)block;
        
        // Читаем размер (octal string -> int)
        size_t size = strtol(h->size, NULL, 8);
        
        // Формируем имя (prefix + name)
        char fullpath[257];
        if (h->prefix[0]) snprintf(fullpath, 256, "/%s/%s", h->prefix, h->name);
        else snprintf(fullpath, 256, "/%s", h->name);

        // Убираем trailing slash, если есть
        size_t len = strlen(fullpath);
        if (len > 1 && fullpath[len-1] == '/') fullpath[len-1] = '\0';

        // Добавляем в список
        struct tar_entry *new_node = malloc(sizeof(struct tar_entry));
        strcpy(new_node->path, fullpath);
        new_node->size = size;
        new_node->data_offset = current_offset + 512;
        new_node->is_dir = (h->typeflag == '5' || size == 0); // 5 = directory
        new_node->next = head;
        head = new_node;

        // Пропускаем данные файла
        size_t blocks_to_skip = (size + 511) / 512;
        fseek(f, blocks_to_skip * 512, SEEK_CUR);
        current_offset += 512 + (blocks_to_skip * 512);
    }
    fclose(f);
}

// Поиск в списке
struct tar_entry *find_entry(const char *path) {
    struct tar_entry *curr = head;
    while (curr) {
        if (strcmp(curr->path, path) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

// --- FUSE Operations ---

static int tar_getattr(const char *path, struct stat *st, struct fuse_file_info *fi) {
    (void) fi;
    memset(st, 0, sizeof(struct stat));
    
    if (strcmp(path, "/") == 0) {
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
        return 0;
    }

    struct tar_entry *e = find_entry(path);
    if (e) {
        if (e->is_dir) {
            st->st_mode = S_IFDIR | 0755;
            st->st_nlink = 2;
        } else {
            st->st_mode = S_IFREG | 0444; // Read only
            st->st_nlink = 1;
            st->st_size = e->size;
        }
        return 0;
    }
    return -ENOENT;
}

static int tar_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    struct tar_entry *curr = head;
    size_t path_len = strlen(path);
    
    // Очень простая логика отображения:
    // Показывает файлы, которые начинаются с path
    while (curr) {
        // Проверка: файл внутри этой папки?
        // Пример: path="/dir", curr="/dir/file" -> OK
        if (strncmp(curr->path, path, path_len) == 0) {
            char *subpath = curr->path + path_len;
            if (subpath[0] == '/') subpath++; // убрать начальный слэш
            
            // Если в остатке пути нет больше слэшей, значит это прямой потомок
            if (subpath[0] != '\0' && strchr(subpath, '/') == NULL) {
                filler(buf, subpath, NULL, 0, 0);
            }
        }
        curr = curr->next;
    }
    return 0;
}

static int tar_open(const char *path, struct fuse_file_info *fi) {
    if ((fi->flags & O_ACCMODE) != O_RDONLY) return -EACCES;
    struct tar_entry *e = find_entry(path);
    if (!e) return -ENOENT;
    return 0;
}

static int tar_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi) {
    struct tar_entry *e = find_entry(path);
    if (!e) return -ENOENT;

    if (offset >= e->size) return 0;
    if (offset + size > e->size) size = e->size - offset;

    // Читаем из оригинального .tar файла, прыгая по смещению
    int res = pread(tar_fd, buf, size, e->data_offset + offset);
    if (res == -1) return -errno;
    return res;
}

static struct fuse_operations operations = {
    .getattr = tar_getattr,
    .readdir = tar_readdir,
    .open    = tar_open,
    .read    = tar_read,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <archive.tar> <mount_point>\n", argv[0]);
        return 1;
    }

    tar_path = realpath(argv[1], NULL);
    // Предварительный парсинг
    printf("Parsing TAR file: %s\n", tar_path);
    parse_tar(tar_path);

    // Открываем fd для операций чтения данных
    tar_fd = open(tar_path, O_RDONLY);

    char *fuse_argv[] = { argv[0], argv[2], "-f", "-s", NULL }; // Single threaded
    return fuse_main(4, fuse_argv, &operations, NULL);
}
