#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <archive.h>
#include <archive_entry.h>

/* Глобальная переменная для хранения пути к архиву */
static char *archive_path = NULL;

/* Структура для хранения информации о файле в архиве */
struct archive_file {
    char *path;        // Полный путь в архиве
    char *name;        // Имя файла
    char *parent;      // Родительская директория
    size_t size;
    char *data;
    int is_dir;
    struct archive_file *next;
};

static struct archive_file *file_list = NULL;

/* Вспомогательная функция: получить текущий timestamp для логов */
static void log_operation(const char *op, const char *path, int result) {
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    fprintf(stderr, "[%s] %s: %s (result: %d)\n", timestamp, op, path, result);
}

/* Функция для нормализации пути - убираем завершающий / */
static char *normalize_path(const char *path) {
    if (path == NULL) return NULL;
    
    char *result = strdup(path);
    size_t len = strlen(result);
    
    if (len > 1 && result[len-1] == '/') {
        result[len-1] = '\0';
    }
    
    return result;
}

/* Функция для получения родительской директории */
static char *get_parent_dir(const char *path) {
    if (path == NULL) return strdup("");
    
    char *normalized = normalize_path(path);
    char *last_slash = strrchr(normalized, '/');
    
    if (last_slash == NULL) {
        free(normalized);
        return strdup("");  // Файл в корне
    }
    
    // Выделяем родительскую директорию
    size_t parent_len = last_slash - normalized;
    char *parent = malloc(parent_len + 1);
    strncpy(parent, normalized, parent_len);
    parent[parent_len] = '\0';
    
    free(normalized);
    return parent;
}

/* Функция для получения имени файла из пути */
static char *get_basename(const char *path) {
    if (path == NULL) return strdup("");
    
    char *normalized = normalize_path(path);
    char *last_slash = strrchr(normalized, '/');
    char *basename;
    
    if (last_slash == NULL) {
        basename = strdup(normalized);
    } else {
        basename = strdup(last_slash + 1);
    }
    
    free(normalized);
    return basename;
}

/* Функция для добавления файла в список */
static void add_file(const char *path, mode_t mode, off_t size, const char *data) {
    struct archive_file *new_file = malloc(sizeof(struct archive_file));
    
    new_file->path = normalize_path(path);
    new_file->name = get_basename(path);
    new_file->parent = get_parent_dir(path);
    new_file->size = size;
    new_file->is_dir = S_ISDIR(mode);
    new_file->next = file_list;
    
    if (data && size > 0 && !new_file->is_dir) {
        new_file->data = malloc(size);
        memcpy(new_file->data, data, size);
    } else {
        new_file->data = NULL;
    }
    
    file_list = new_file;
}

/* Функция для поиска файла по пути */
static struct archive_file *find_file(const char *path) {
    // Специальная обработка корневой директории
    if (strcmp(path, "/") == 0) {
        return NULL; // Корневая директория обрабатывается отдельно
    }
    
    // Пропускаем ведущий '/'
    const char *search_path = (path[0] == '/') ? path + 1 : path;
    if (strlen(search_path) == 0) {
        return NULL;
    }
    
    char *normalized_search = normalize_path(search_path);
    
    struct archive_file *current = file_list;
    while (current != NULL) {
        if (strcmp(current->path, normalized_search) == 0) {
            free(normalized_search);
            return current;
        }
        current = current->next;
    }
    
    free(normalized_search);
    return NULL;
}

/* Функция для получения файлов в директории */
static int get_files_in_dir(const char *dir_path, struct archive_file **results, int max_results) {
    int count = 0;
    
    // Нормализуем путь директории
    char *normalized_dir;
    if (strcmp(dir_path, "/") == 0) {
        normalized_dir = strdup(".");
    } else {
        normalized_dir = (dir_path[0] == '/') ? strdup(dir_path + 1) : strdup(dir_path);
    }
    
    if (strlen(normalized_dir) == 0) {
        free(normalized_dir);
        normalized_dir = strdup(".");
    }
    
    struct archive_file *current = file_list;
    while (current != NULL && count < max_results) {
        // Для корневой директории показываем файлы без родителя или с родителем "."
        if (strcmp(normalized_dir, ".") == 0) {
            if (strlen(current->parent) == 0 || strcmp(current->parent, ".") == 0) {
                results[count++] = current;
            }
        } else {
            // Для поддиректорий ищем точное совпадение родителя
            if (strcmp(current->parent, normalized_dir) == 0) {
                results[count++] = current;
            }
        }
        current = current->next;
    }
    
    free(normalized_dir);
    return count;
}

/* Функция для чтения и парсинга tar архива */
static int read_archive(const char *path) {
    struct archive *a;
    struct archive_entry *entry;
    int r;

    a = archive_read_new();
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);
    
    r = archive_read_open_filename(a, path, 10240);
    if (r != ARCHIVE_OK) {
        fprintf(stderr, "Error opening archive: %s\n", archive_error_string(a));
        archive_read_free(a);
        return -1;
    }

    fprintf(stderr, "Reading archive: %s\n", path);
    int file_count = 0;

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char *entry_path = archive_entry_pathname(entry);
        mode_t mode = archive_entry_mode(entry);
        off_t size = archive_entry_size(entry);
        
        // Читаем данные для обычных файлов
        char *file_data = NULL;
        if (!S_ISDIR(mode) && size > 0) {
            file_data = malloc(size);
            ssize_t read_size = archive_read_data(a, file_data, size);
            if (read_size != size) {
                fprintf(stderr, "Warning: read %zd of %ld bytes for %s\n", 
                        read_size, size, entry_path);
                free(file_data);
                file_data = NULL;
            }
        } else {
            archive_read_data_skip(a);
        }

        add_file(entry_path, mode, size, file_data);
        
        if (file_data) free(file_data);
        file_count++;
    }

    archive_read_close(a);
    archive_read_free(a);
    
    fprintf(stderr, "Archive loaded: %d files/directories\n", file_count);
    return 0;
}

/*
 * getattr - получить атрибуты файла
 */
static int archive_getattr(const char *path, struct stat *stbuf,
                          struct fuse_file_info *fi) {
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));
    
    /* Корневая директория */
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        stbuf->st_size = 4096;
        log_operation("GETATTR", path, 0);
        return 0;
    }
    
    /* Ищем файл в архиве */
    struct archive_file *file = find_file(path);
    if (file == NULL) {
        log_operation("GETATTR", path, -ENOENT);
        return -ENOENT;
    }
    
    if (file->is_dir) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        stbuf->st_size = 4096;
    } else {
        stbuf->st_mode = S_IFREG | 0444;  // read-only
        stbuf->st_nlink = 1;
        stbuf->st_size = file->size;
    }
    
    log_operation("GETATTR", path, 0);
    return 0;
}

/*
 * readdir - прочитать содержимое директории
 */
static int archive_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                          off_t offset, struct fuse_file_info *fi,
                          enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    /* Всегда добавляем . и .. */
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    /* Получаем файлы в текущей директории */
    struct archive_file *files[100];
    int file_count = get_files_in_dir(path, files, 100);
    
    for (int i = 0; i < file_count; i++) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        
        if (files[i]->is_dir) {
            st.st_mode = S_IFDIR | 0755;
            st.st_nlink = 2;
        } else {
            st.st_mode = S_IFREG | 0444;
            st.st_nlink = 1;
            st.st_size = files[i]->size;
        }
        
        filler(buf, files[i]->name, &st, 0, 0);
    }

    log_operation("READDIR", path, file_count);
    return 0;
}

/*
 * open - открыть файл (только для чтения)
 */
static int archive_open(const char *path, struct fuse_file_info *fi) {
    /* Проверяем, что файл существует */
    if (strcmp(path, "/") == 0) {
        log_operation("OPEN", path, -EISDIR);
        return -EISDIR;
    }
    
    struct archive_file *file = find_file(path);
    if (file == NULL) {
        log_operation("OPEN", path, -ENOENT);
        return -ENOENT;
    }
    
    /* Проверяем, что это не директория */
    if (file->is_dir) {
        log_operation("OPEN", path, -EISDIR);
        return -EISDIR;
    }
    
    /* Проверяем права доступа (только чтение) */
    if ((fi->flags & O_ACCMODE) != O_RDONLY) {
        log_operation("OPEN", path, -EACCES);
        return -EACCES;
    }
    
    log_operation("OPEN", path, 0);
    return 0;
}

/*
 * read - прочитать данные из файла
 */
static int archive_read(const char *path, char *buf, size_t size, off_t offset,
                       struct fuse_file_info *fi) {
    (void) fi;
    
    if (strcmp(path, "/") == 0) {
        log_operation("READ", path, -EISDIR);
        return -EISDIR;
    }
    
    struct archive_file *file = find_file(path);
    if (file == NULL) {
        log_operation("READ", path, -ENOENT);
        return -ENOENT;
    }
    
    if (file->is_dir) {
        log_operation("READ", path, -EISDIR);
        return -EISDIR;
    }
    
    /* Проверяем границы */
    if (offset >= file->size) {
        return 0;
    }
    
    if (offset + size > file->size) {
        size = file->size - offset;
    }
    
    /* Копируем данные из архива */
    if (file->data != NULL) {
        memcpy(buf, file->data + offset, size);
    }
    
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(stderr, "[%s] READ: %s (%zu bytes at offset %ld, result: %zu)\n",
            timestamp, path, size, offset, size);
    
    return size;
}

/* Структура операций FUSE (только чтение) */
static struct fuse_operations archive_oper = {
    .getattr    = archive_getattr,
    .readdir    = archive_readdir,
    .open       = archive_open,
    .read       = archive_read,
};

int main(int argc, char *argv[]) {
    /* Проверка аргументов */
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <archive.tar> <mount_point> [fuse_options]\n", argv[0]);
        fprintf(stderr, "Example: %s /path/to/archive.tar /mnt/archive -f\n", argv[0]);
        fprintf(stderr, "\nOptions:\n");
        fprintf(stderr, "  -f  foreground mode (see logs)\n");
        fprintf(stderr, "  -d  debug mode (detailed FUSE output)\n");
        fprintf(stderr, "  -s  single-threaded mode\n");
        return 1;
    }

    /* Сохраняем путь к архиву */
    archive_path = realpath(argv[1], NULL);
    if (archive_path == NULL) {
        perror("realpath");
        fprintf(stderr, "Error: Archive file '%s' not found\n", argv[1]);
        return 1;
    }

    /* Проверяем, что это файл */
    struct stat st;
    if (stat(archive_path, &st) == -1 || !S_ISREG(st.st_mode)) {
        fprintf(stderr, "Error: '%s' is not a regular file\n", argv[1]);
        free(archive_path);
        return 1;
    }

    /* Читаем архив */
    if (read_archive(archive_path) != 0) {
        fprintf(stderr, "Failed to read archive\n");
        free(archive_path);
        return 1;
    }

    fprintf(stderr, "=== FUSE Archive Filesystem ===\n");
    fprintf(stderr, "Archive: %s\n", archive_path);
    fprintf(stderr, "Mount:   %s\n", argv[2]);
    fprintf(stderr, "To unmount: fusermount -u %s\n\n", argv[2]);

    /* Запускаем FUSE */
    int fuse_argc = argc - 1;
    char **fuse_argv = malloc(sizeof(char*) * (fuse_argc + 1));
    fuse_argv[0] = argv[0];
    for (int i = 2; i < argc; i++) {
        fuse_argv[i-1] = argv[i];
    }
    fuse_argv[fuse_argc] = NULL;

    int ret = fuse_main(fuse_argc, fuse_argv, &archive_oper, NULL);

    /* Освобождаем память */
    struct archive_file *current = file_list;
    while (current != NULL) {
        struct archive_file *next = current->next;
        free(current->path);
        free(current->name);
        free(current->parent);
        if (current->data) {
            free(current->data);
        }
        free(current);
        current = next;
    }
    
    free(fuse_argv);
    free(archive_path);

    return ret;
}
