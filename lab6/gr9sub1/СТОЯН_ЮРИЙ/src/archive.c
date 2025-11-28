#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <archive.h>
#include <archive_entry.h>
#include <sys/stat.h>
#include <limits.h>

#define MAX_FILES 1024

// Структура для хранения информации о файле в архиве
typedef struct {
    char path[PATH_MAX];
    mode_t mode;
    size_t size;
    off_t offset;       // Смещение данных в архиве
    time_t mtime;
} archive_file_t;

// Глобальные данные
static char *archive_path = NULL;
static archive_file_t files[MAX_FILES];
static int file_count = 0;

/**
 * Загрузка метаданных из tar архива
 */
static int load_archive_metadata() {
    struct archive *a = archive_read_new();
    struct archive_entry *entry;
    
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    
    if (archive_read_open_filename(a, archive_path, 10240) != ARCHIVE_OK) {
        fprintf(stderr, "Failed to open archive: %s\n", archive_error_string(a));
        archive_read_free(a);
        return -1;
    }
    
    file_count = 0;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        if (file_count >= MAX_FILES) {
            fprintf(stderr, "Too many files in archive\n");
            break;
        }
        
        const char *pathname = archive_entry_pathname(entry);
        
        // Пропускаем директорию "." и пустые пути
        if (strcmp(pathname, ".") == 0 || strcmp(pathname, "./") == 0 || strlen(pathname) == 0) {
            archive_read_data_skip(a);
            continue;
        }
        
        // Нормализация пути: убираем ./ в начале и добавляем /
        const char *clean_path = pathname;
        if (strncmp(pathname, "./", 2) == 0) {
            clean_path = pathname + 2;
        }
        
        // Пропускаем пустые пути после очистки
        if (strlen(clean_path) == 0) {
            archive_read_data_skip(a);
            continue;
        }
        
        // Добавляем / в начало если нужно
        if (clean_path[0] != '/') {
            snprintf(files[file_count].path, PATH_MAX, "/%s", clean_path);
        } else {
            strncpy(files[file_count].path, clean_path, PATH_MAX - 1);
        }
        
        // Удаляем завершающий / для директорий в пути
        size_t len = strlen(files[file_count].path);
        if (len > 1 && files[file_count].path[len - 1] == '/') {
            files[file_count].path[len - 1] = '\0';
        }
        
        files[file_count].mode = archive_entry_mode(entry);
        files[file_count].size = archive_entry_size(entry);
        files[file_count].mtime = archive_entry_mtime(entry);
        files[file_count].offset = archive_read_header_position(a);
        
        fprintf(stderr, "Loaded: %s (size: %zu, mode: %o, type: %s)\n", 
                files[file_count].path, files[file_count].size, files[file_count].mode,
                S_ISDIR(files[file_count].mode) ? "DIR" : "FILE");
        
        file_count++;
        archive_read_data_skip(a);
    }
    
    archive_read_free(a);
    fprintf(stderr, "Total files loaded: %d\n", file_count);
    return 0;
}

/**
 * Поиск файла по пути
 */
static archive_file_t* find_file(const char *path) {
    for (int i = 0; i < file_count; i++) {
        if (strcmp(files[i].path, path) == 0) {
            return &files[i];
        }
    }
    return NULL;
}

/**
 * Получение атрибутов файла
 */
static int archive_getattr(const char *path, struct stat *stbuf,
                          struct fuse_file_info *fi) {
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));
    
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }
    
    archive_file_t *file = find_file(path);
    if (file == NULL) {
        // Проверяем, является ли это директорией
        size_t path_len = strlen(path);
        for (int i = 0; i < file_count; i++) {
            if (strncmp(files[i].path, path, path_len) == 0 &&
                files[i].path[path_len] == '/' &&
                strlen(files[i].path) > path_len) {
                stbuf->st_mode = S_IFDIR | 0755;
                stbuf->st_nlink = 2;
                return 0;
            }
        }
        return -ENOENT;
    }
    
    stbuf->st_mode = file->mode;
    stbuf->st_nlink = 1;
    stbuf->st_size = file->size;
    stbuf->st_mtime = file->mtime;
    
    return 0;
}

/**
 * Чтение содержимого директории
 */
static int archive_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                          off_t offset, struct fuse_file_info *fi,
                          enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;
    
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    
    size_t path_len = strlen(path);
    int is_root = (strcmp(path, "/") == 0);
    
    // Массив для отслеживания уже добавленных имён (чтобы избежать дубликатов)
    char added_names[MAX_FILES][256];
    int added_count = 0;
    
    for (int i = 0; i < file_count; i++) {
        const char *file_path = files[i].path;
        
        // Определяем, находится ли файл в нужной директории
        int in_dir = 0;
        const char *name = NULL;
        
        if (is_root) {
            // Для корневой директории берём всё после первого /
            if (file_path[0] == '/' && file_path[1] != '\0') {
                name = file_path + 1;
                in_dir = 1;
            }
        } else {
            // Для поддиректорий проверяем префикс
            if (strncmp(file_path, path, path_len) == 0) {
                if (file_path[path_len] == '/' && file_path[path_len + 1] != '\0') {
                    name = file_path + path_len + 1;
                    in_dir = 1;
                } else if (file_path[path_len] == '\0') {
                    // Это сама директория
                    continue;
                }
            }
        }
        
        if (!in_dir || name == NULL) {
            continue;
        }
        
        // Проверяем, есть ли слэш в имени (значит это в поддиректории)
        const char *slash = strchr(name, '/');
        char display_name[256];
        int is_directory = 0;
        
        if (slash != NULL) {
            // Это поддиректория - берём только имя директории
            size_t dir_len = slash - name;
            if (dir_len >= sizeof(display_name)) {
                dir_len = sizeof(display_name) - 1;
            }
            strncpy(display_name, name, dir_len);
            display_name[dir_len] = '\0';
            is_directory = 1;
        } else {
            // Это файл или директория в текущей директории
            strncpy(display_name, name, sizeof(display_name) - 1);
            display_name[sizeof(display_name) - 1] = '\0';
            is_directory = S_ISDIR(files[i].mode);
        }
        
        // Проверяем, не добавили ли мы уже этот элемент
        int already_added = 0;
        for (int j = 0; j < added_count; j++) {
            if (strcmp(added_names[j], display_name) == 0) {
                already_added = 1;
                break;
            }
        }
        
        if (!already_added && added_count < MAX_FILES) {
            struct stat st;
            memset(&st, 0, sizeof(st));
            st.st_mode = is_directory ? (S_IFDIR | 0755) : (S_IFREG | 0644);
            
            filler(buf, display_name, &st, 0, 0);
            
            strncpy(added_names[added_count], display_name, sizeof(added_names[0]) - 1);
            added_names[added_count][sizeof(added_names[0]) - 1] = '\0';
            added_count++;
        }
    }
    
    return 0;
}

/**
 * Открытие файла
 */
static int archive_open(const char *path, struct fuse_file_info *fi) {
    archive_file_t *file = find_file(path);
    if (file == NULL) {
        return -ENOENT;
    }
    
    if ((fi->flags & O_ACCMODE) != O_RDONLY) {
        return -EACCES;
    }
    
    return 0;
}

/**
 * Чтение данных из файла
 */
static int archive_read(const char *path, char *buf, size_t size, off_t offset,
                       struct fuse_file_info *fi) {
    (void) fi;
    
    archive_file_t *file = find_file(path);
    if (file == NULL) {
        return -ENOENT;
    }
    
    if (offset >= (off_t)file->size) {
        return 0;
    }
    
    if (offset + size > file->size) {
        size = file->size - offset;
    }
    
    // Открываем архив и читаем данные
    struct archive *a = archive_read_new();
    struct archive_entry *entry;
    
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    
    if (archive_read_open_filename(a, archive_path, 10240) != ARCHIVE_OK) {
        archive_read_free(a);
        return -EIO;
    }
    
    int found = 0;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char *pathname = archive_entry_pathname(entry);
        
        // Пропускаем "." и "./"
        if (strcmp(pathname, ".") == 0 || strcmp(pathname, "./") == 0) {
            archive_read_data_skip(a);
            continue;
        }
        
        // Нормализация пути
        const char *clean_path = pathname;
        if (strncmp(pathname, "./", 2) == 0) {
            clean_path = pathname + 2;
        }
        
        if (strlen(clean_path) == 0) {
            archive_read_data_skip(a);
            continue;
        }
        
        char normalized_path[PATH_MAX];
        if (clean_path[0] != '/') {
            snprintf(normalized_path, PATH_MAX, "/%s", clean_path);
        } else {
            strncpy(normalized_path, clean_path, PATH_MAX - 1);
        }
        
        // Удаляем завершающий /
        size_t len = strlen(normalized_path);
        if (len > 1 && normalized_path[len - 1] == '/') {
            normalized_path[len - 1] = '\0';
        }
        
        if (strcmp(normalized_path, path) == 0) {
            found = 1;
            
            // Пропускаем offset байт
            if (offset > 0) {
                char skip_buf[4096];
                size_t remaining = offset;
                while (remaining > 0) {
                    size_t to_read = remaining < sizeof(skip_buf) ? remaining : sizeof(skip_buf);
                    ssize_t r = archive_read_data(a, skip_buf, to_read);
                    if (r <= 0) break;
                    remaining -= r;
                }
            }
            
            // Читаем нужные данные
            ssize_t bytes_read = archive_read_data(a, buf, size);
            archive_read_free(a);
            return bytes_read > 0 ? bytes_read : 0;
        }
        
        archive_read_data_skip(a);
    }
    
    archive_read_free(a);
    return found ? -EIO : -ENOENT;
}

/**
 * Таблица операций FUSE
 */
static struct fuse_operations archive_oper = {
    .getattr = archive_getattr,
    .readdir = archive_readdir,
    .open = archive_open,
    .read = archive_read,
};

/**
 * Главная функция
 */
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <archive.tar> <mount_point>\n", argv[0]);
        return 1;
    }
    
    archive_path = realpath(argv[1], NULL);
    if (archive_path == NULL) {
        perror("realpath");
        return 1;
    }
    
    fprintf(stderr, "Loading archive: %s\n", archive_path);
    if (load_archive_metadata() != 0) {
        free(archive_path);
        return 1;
    }
    
    // Удаляем первый аргумент для FUSE
    for (int i = 1; i < argc - 1; i++) {
        argv[i] = argv[i + 1];
    }
    argc--;
    
    fprintf(stderr, "Mounting archive to %s\n", argv[1]);
    
    int ret = fuse_main(argc, argv, &archive_oper, NULL);
    
    free(archive_path);
    return ret;
}