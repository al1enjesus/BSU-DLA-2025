#define FUSE_USE_VERSION 31
#include "archive_ops.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <archive.h>
#include <archive_entry.h>
#include <ctype.h>
#include <libgen.h>

/* ========== БЕЗОПАСНАЯ ОБРАБОТКА ПУТЕЙ ========== */

// Безопасная нормализация пути с защитой от traversal attacks
char* safe_normalize_path(const char *path) {
    if (!path) {
        return NULL;
    }
    
    // Пропускаем ведущий слеш
    if (path[0] == '/') {
        path++;
    }
    
    // Если путь пустой - это корень
    if (*path == '\0') {
        return strdup("");
    }
    
    // Убираем ведущие "./" если есть
    while (strncmp(path, "./", 2) == 0) {
        path += 2;
    }
    
    // Проверяем наличие опасных компонентов
    char *path_copy = strdup(path);
    if (!path_copy) {
        return NULL;
    }
    
    // Разбиваем на компоненты
    char *components[256];
    int component_count = 0;
    char *saveptr = NULL;
    char *token = strtok_r(path_copy, "/", &saveptr);
    
    while (token != NULL && component_count < 256) {
        // Пропускаем пустые компоненты и "."
        if (strcmp(token, "") == 0 || strcmp(token, ".") == 0) {
            token = strtok_r(NULL, "/", &saveptr);
            continue;
        }
        
        // Запрещаем ".."
        if (strcmp(token, "..") == 0) {
            free(path_copy);
            return NULL;
        }
        
        components[component_count++] = token;
        token = strtok_r(NULL, "/", &saveptr);
    }
    
    // Собираем нормализованный путь
    char *result = malloc(4096);
    if (!result) {
        free(path_copy);
        return NULL;
    }
    
    result[0] = '\0';
    
    for (int i = 0; i < component_count; i++) {
        if (i > 0) {
            strcat(result, "/");
        }
        strcat(result, components[i]);
    }
    
    free(path_copy);
    return result;
}

// Проверка безопасности пути
static int is_path_safe(const char *path) {
    char *normalized = safe_normalize_path(path);
    if (!normalized) {
        return 0;
    }
    free(normalized);
    return 1;
}

// Нахождение файла по пути (потокобезопасно)
struct archive_file* find_file(struct archive_data *data, const char *path) {
    if (!data || !path) {
        return NULL;
    }
    
    // Нормализуем путь
    char *normalized = safe_normalize_path(path);
    if (!normalized) {
        return NULL;
    }
    
    struct archive_file *result = NULL;
    struct archive_file *file = data->files;
    
    while (file) {
        if (strcmp(file->path, normalized) == 0) {
            result = file;
            break;
        }
        file = file->next;
    }
    
    free(normalized);
    return result;
}

/* ========== РАБОТА СО СПИСКОМ ФАЙЛОВ ========== */

// Добавление файла в список
static void add_file_to_list(struct archive_data *data, 
                            const char *path, 
                            off_t size, 
                            off_t offset,
                            mode_t mode,
                            time_t mtime,
                            uid_t uid,
                            gid_t gid,
                            int is_dir) {
    
    char *normalized = safe_normalize_path(path);
    if (!normalized) {
        fprintf(stderr, "WARNING: Skipping unsafe path: %s\n", path);
        return;
    }
    
    // Проверяем, не существует ли уже такой записи
    struct archive_file *existing = data->files;
    while (existing) {
        if (strcmp(existing->path, normalized) == 0) {
            free(normalized);
            return;  // Уже существует
        }
        existing = existing->next;
    }
    
    struct archive_file *file = malloc(sizeof(struct archive_file));
    if (!file) {
        fprintf(stderr, "ERROR: Failed to allocate memory for file entry\n");
        free(normalized);
        return;
    }
    
    file->path = normalized;
    file->size = size;
    file->offset = offset;
    file->mode = mode;
    file->mtime = mtime;
    file->uid = uid;
    file->gid = gid;
    file->is_dir = is_dir;
    file->next = NULL;
    
    // Добавляем в конец списка для сохранения порядка
    if (!data->files) {
        data->files = file;
    } else {
        struct archive_file *last = data->files;
        while (last->next) {
            last = last->next;
        }
        last->next = file;
    }
    
    data->file_count++;
    if (is_dir) {
        data->dir_count++;
    }
    
    fprintf(stderr, "DEBUG: Added %s: %s (size=%ld, offset=%ld)\n", 
            is_dir ? "DIR " : "FILE", file->path, file->size, file->offset);
}

// Создание родительских директорий
static void create_parent_directories(struct archive_data *data, 
                                     const char *path,
                                     time_t mtime,
                                     uid_t uid,
                                     gid_t gid) {
    
    char *normalized = safe_normalize_path(path);
    if (!normalized) {
        return;
    }
    
    // Разбиваем путь на компоненты
    char *path_copy = strdup(normalized);
    char *components[256];
    int component_count = 0;
    char *saveptr = NULL;
    char *token = strtok_r(path_copy, "/", &saveptr);
    
    while (token != NULL) {
        components[component_count++] = token;
        token = strtok_r(NULL, "/", &saveptr);
    }
    
    // Создаем пути для всех родительских директорий
    char current_path[4096] = "";
    
    for (int i = 0; i < component_count - 1; i++) {
        if (i > 0) {
            strcat(current_path, "/");
        }
        strcat(current_path, components[i]);
        
        // Проверяем, существует ли уже эта директория
        struct archive_file *existing = data->files;
        int exists = 0;
        while (existing) {
            if (strcmp(existing->path, current_path) == 0 && existing->is_dir) {
                exists = 1;
                break;
            }
            existing = existing->next;
        }
        
        if (!exists) {
            add_file_to_list(data, current_path, 4096, 0, 
                            S_IFDIR | 0755, mtime, uid, gid, 1);
        }
    }
    
    free(path_copy);
    free(normalized);
}

/* ========== ПАРСИНГ TAR АРХИВА ========== */

int parse_tar_archive(struct archive_data *data) {
    fprintf(stderr, "DEBUG: Parsing tar archive...\n");
    
    struct archive *a = archive_read_new();
    struct archive_entry *entry;
    int r;
    
    if (!a) {
        fprintf(stderr, "ERROR: Failed to create archive object\n");
        return -ENOMEM;
    }
    
    // Поддерживаемые форматы и фильтры
    archive_read_support_format_tar(a);
    archive_read_support_filter_all(a);
    
    // Открываем архив
    r = archive_read_open_fd(a, data->fd, 10240);
    if (r != ARCHIVE_OK) {
        fprintf(stderr, "ERROR: Failed to open archive: %s\n", archive_error_string(a));
        archive_read_free(a);
        return -EIO;
    }
    
    int entries_processed = 0;
    
    while (1) {
        r = archive_read_next_header(a, &entry);
        if (r == ARCHIVE_EOF) {
            fprintf(stderr, "DEBUG: End of archive reached\n");
            break;
        }
        
        if (r != ARCHIVE_OK) {
            fprintf(stderr, "ERROR reading archive header: %s\n", archive_error_string(a));
            break;
        }
        
        const char *pathname = archive_entry_pathname(entry);
        if (!pathname) {
            fprintf(stderr, "WARNING: Skipping entry with NULL pathname\n");
            continue;
        }
        
        // Пропускаем специальные файлы
        mode_t type = archive_entry_filetype(entry);
        if (type == AE_IFLNK || type == AE_IFCHR || 
            type == AE_IFBLK || type == AE_IFIFO || type == AE_IFSOCK) {
            fprintf(stderr, "DEBUG: Skipping special file: %s\n", pathname);
            continue;
        }
        
        int is_dir = (type == AE_IFDIR);
        
        fprintf(stderr, "DEBUG: Processing entry: %s (type=%c)\n", 
                pathname, is_dir ? 'D' : 'F');
        
        // Создаем родительские директории
        if (!is_dir) {
            create_parent_directories(data, pathname, 
                                     archive_entry_mtime(entry),
                                     archive_entry_uid(entry),
                                     archive_entry_gid(entry));
        }
        
        // Получаем информацию о файле
        off_t size = is_dir ? 4096 : archive_entry_size(entry);
        mode_t mode = archive_entry_mode(entry);
        time_t mtime = archive_entry_mtime(entry);
        uid_t uid = archive_entry_uid(entry);
        gid_t gid = archive_entry_gid(entry);
        
        // Для файлов получаем смещение данных
        off_t data_offset = 0;
        if (!is_dir) {
            data_offset = archive_read_header_position(a);
            if (data_offset > 0) {
                // Данные начинаются через 512 байт после заголовка
                data_offset += 512;
            }
            
            // Пропускаем данные файла
            r = archive_read_data_skip(a);
            if (r != ARCHIVE_OK) {
                fprintf(stderr, "WARNING: Failed to skip data for %s: %s\n", 
                        pathname, archive_error_string(a));
            }
        }
        
        // Добавляем запись
        add_file_to_list(data, pathname, size, data_offset, mode,
                        mtime, uid, gid, is_dir);
        
        entries_processed++;
    }
    
    archive_read_free(a);
    
    // Добавляем корневую директорию, если ее еще нет
    struct archive_file *existing_root = find_file(data, "/");
    if (!existing_root) {
        add_file_to_list(data, "", 4096, 0, S_IFDIR | 0755, 
                        time(NULL), getuid(), getgid(), 1);
    }
    
    fprintf(stderr, "DEBUG: Successfully parsed %d entries\n", entries_processed);
    fprintf(stderr, "DEBUG: Total entries: %d (files: %d, dirs: %d)\n", 
            data->file_count, 
            data->file_count - data->dir_count,
            data->dir_count);
    
    return 0;
}

/* ========== FUSE ОПЕРАЦИИ ========== */

void* archive_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
    (void)conn;
    
    struct archive_data *data = fuse_get_context()->private_data;
    
    fprintf(stderr, "DEBUG: Initializing archive filesystem...\n");
    
    // Открываем архив
    data->fd = open(data->archive_path, O_RDONLY);
    if (data->fd < 0) {
        fprintf(stderr, "ERROR: Cannot open archive '%s': %s\n", 
                data->archive_path, strerror(errno));
        return NULL;
    }
    
    // Инициализируем структуры
    data->files = NULL;
    data->file_count = 0;
    data->dir_count = 0;
    pthread_mutex_init(&data->mutex, NULL);
    
    // Парсим архив
    int ret = parse_tar_archive(data);
    if (ret != 0) {
        fprintf(stderr, "ERROR: Failed to parse archive\n");
        close(data->fd);
        pthread_mutex_destroy(&data->mutex);
        return NULL;
    }
    
    // Настройки FUSE
    cfg->kernel_cache = 1;
    cfg->attr_timeout = 5.0;
    cfg->entry_timeout = 5.0;
    cfg->negative_timeout = 5.0;
    
    fprintf(stderr, "DEBUG: Archive filesystem initialized successfully\n");
    
    return data;
}

void archive_destroy(void *private_data) {
    struct archive_data *data = private_data;
    
    if (!data) {
        return;
    }
    
    fprintf(stderr, "DEBUG: Destroying archive filesystem...\n");
    
    if (data->fd >= 0) {
        close(data->fd);
    }
    
    // Освобождаем список файлов
    struct archive_file *file = data->files;
    while (file) {
        struct archive_file *next = file->next;
        free(file->path);
        free(file);
        file = next;
    }
    
    pthread_mutex_destroy(&data->mutex);
    free(data->archive_path);
    free(data);
    
    fprintf(stderr, "DEBUG: Archive filesystem destroyed\n");
}

int archive_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void)fi;
    
    struct archive_data *data = fuse_get_context()->private_data;
    
    memset(stbuf, 0, sizeof(struct stat));
    
    pthread_mutex_lock(&data->mutex);
    
    // Проверяем корневую директорию
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        stbuf->st_size = 4096;
        stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);
        stbuf->st_blksize = 4096;
        stbuf->st_blocks = 8;
        pthread_mutex_unlock(&data->mutex);
        return 0;
    }
    
    // Проверяем безопасность пути
    if (!is_path_safe(path)) {
        pthread_mutex_unlock(&data->mutex);
        return -EACCES;
    }
    
    // Ищем файл
    struct archive_file *file = find_file(data, path);
    if (!file) {
        pthread_mutex_unlock(&data->mutex);
        return -ENOENT;
    }
    
    // Заполняем структуру stat
    if (file->is_dir) {
        stbuf->st_mode = file->mode;
        if ((stbuf->st_mode & S_IFMT) != S_IFDIR) {
            stbuf->st_mode = (stbuf->st_mode & ~S_IFMT) | S_IFDIR;
        }
        stbuf->st_nlink = 2;
        stbuf->st_size = 4096;
    } else {
        stbuf->st_mode = file->mode;
        if ((stbuf->st_mode & S_IFMT) != S_IFREG) {
            stbuf->st_mode = (stbuf->st_mode & ~S_IFMT) | S_IFREG;
        }
        stbuf->st_nlink = 1;
        stbuf->st_size = file->size;
    }
    
    stbuf->st_uid = file->uid;
    stbuf->st_gid = file->gid;
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = file->mtime;
    stbuf->st_blksize = 4096;
    stbuf->st_blocks = (file->size + 511) / 512;
    
    pthread_mutex_unlock(&data->mutex);
    return 0;
}

int archive_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                    off_t offset, struct fuse_file_info *fi,
                    enum fuse_readdir_flags flags) {
    (void)offset; (void)fi; (void)flags;
    
    struct archive_data *data = fuse_get_context()->private_data;
    
    // Добавляем стандартные записи
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    
    pthread_mutex_lock(&data->mutex);
    
    // Получаем нормализованный путь директории
    char *dir_path = safe_normalize_path(path);
    if (!dir_path) {
        pthread_mutex_unlock(&data->mutex);
        return -EACCES;
    }
    
    // Проходим по всем файлам и добавляем те, что находятся в этой директории
    struct archive_file *file = data->files;
    
    while (file) {
        // Пропускаем корневую директорию (пустой путь)
        if (strcmp(file->path, "") == 0) {
            file = file->next;
            continue;
        }
        
        // Получаем имя файла и путь к его родительской директории
        char *file_path = file->path;
        char *last_slash = strrchr(file_path, '/');
        
        if (last_slash) {
            // Файл находится в поддиректории
            *last_slash = '\0';
            char *parent_dir = file_path;
            char *filename = last_slash + 1;
            
            if (strcmp(parent_dir, dir_path) == 0 && strlen(filename) > 0) {
                filler(buf, filename, NULL, 0, 0);
            }
            
            *last_slash = '/';  // Восстанавливаем строку
        } else {
            // Файл находится в корне
            if (strcmp(dir_path, "") == 0) {
                filler(buf, file_path, NULL, 0, 0);
            }
        }
        
        file = file->next;
    }
    
    free(dir_path);
    pthread_mutex_unlock(&data->mutex);
    return 0;
}

int archive_open(const char *path, struct fuse_file_info *fi) {
    struct archive_data *data = fuse_get_context()->private_data;
    
    // Проверяем корневую директорию
    if (strcmp(path, "/") == 0) {
        return -EISDIR;
    }
    
    // Проверяем безопасность пути
    if (!is_path_safe(path)) {
        return -EACCES;
    }
    
    pthread_mutex_lock(&data->mutex);
    
    // Ищем файл
    struct archive_file *file = find_file(data, path);
    if (!file) {
        pthread_mutex_unlock(&data->mutex);
        return -ENOENT;
    }
    
    // Проверяем, что это файл, а не директория
    if (file->is_dir) {
        pthread_mutex_unlock(&data->mutex);
        return -EISDIR;
    }
    
    // Проверяем режим доступа (только чтение)
    if ((fi->flags & O_ACCMODE) != O_RDONLY) {
        pthread_mutex_unlock(&data->mutex);
        return -EACCES;
    }
    
    // Сохраняем указатель на файл в fh
    fi->fh = (uint64_t)(uintptr_t)file;
    fi->direct_io = 0;
    fi->keep_cache = 1;
    fi->nonseekable = 0;
    
    pthread_mutex_unlock(&data->mutex);
    return 0;
}

int archive_read(const char *path, char *buf, size_t size, off_t offset,
                 struct fuse_file_info *fi) {
    (void)path;  // Используем fi->fh вместо path
    
    struct archive_data *data = fuse_get_context()->private_data;
    
    if (!data || data->fd < 0) {
        return -EBADF;
    }
    
    pthread_mutex_lock(&data->mutex);
    
    struct archive_file *file = (struct archive_file*)(uintptr_t)fi->fh;
    
    // Проверяем валидность указателя
    if (!file) {
        pthread_mutex_unlock(&data->mutex);
        return -EBADF;
    }
    
    // Проверяем, что смещение не отрицательное
    if (offset < 0) {
        pthread_mutex_unlock(&data->mutex);
        return -EINVAL;
    }
    
    // Проверяем, не вышли ли за пределы файла
    if ((off_t)offset >= file->size) {
        pthread_mutex_unlock(&data->mutex);
        return 0;
    }
    
    // Корректируем размер для чтения
    if ((off_t)(offset + size) > file->size) {
        size = file->size - offset;
    }
    
    if (size == 0) {
        pthread_mutex_unlock(&data->mutex);
        return 0;
    }
    
    // Вычисляем реальное смещение в архиве
    off_t archive_offset = file->offset + offset;
    
    // Читаем данные
    ssize_t bytes_read = pread(data->fd, buf, size, archive_offset);
    
    pthread_mutex_unlock(&data->mutex);
    
    if (bytes_read < 0) {
        int err = errno;
        fprintf(stderr, "ERROR: pread failed: %s\n", strerror(err));
        
        switch (err) {
            case EBADF: return -EBADF;
            case EINVAL: return -EINVAL;
            case EIO: return -EIO;
            case ENOMEM: return -ENOMEM;
            default: return -EIO;
        }
    }
    
    fprintf(stderr, "DEBUG: Read %zd bytes from %s at offset %ld\n", 
            bytes_read, file->path, offset);
    
    return bytes_read;
}

int archive_statfs(const char *path, struct statvfs *stbuf) {
    (void)path;
    
    struct archive_data *data = fuse_get_context()->private_data;
    
    pthread_mutex_lock(&data->mutex);
    
    // Рассчитываем реалистичные значения на основе архива
    off_t total_size = data->archive_size * 2;  // Примерная оценка
    int block_size = 4096;
    
    stbuf->f_bsize = block_size;
    stbuf->f_frsize = block_size;
    stbuf->f_blocks = total_size / block_size;
    stbuf->f_bfree = 0;      // Read-only, нет свободного места
    stbuf->f_bavail = 0;     // Read-only, нет доступного места
    stbuf->f_files = data->file_count;
    stbuf->f_ffree = 0;      // Read-only, нельзя создавать файлы
    stbuf->f_favail = 0;     // Read-only
    stbuf->f_namemax = 255;
    stbuf->f_flag = ST_RDONLY;
    
    pthread_mutex_unlock(&data->mutex);
    return 0;
}

int archive_access(const char *path, int mask) {
    struct archive_data *data = fuse_get_context()->private_data;
    
    // Проверяем корневую директорию
    if (strcmp(path, "/") == 0) {
        if (mask & W_OK) {
            return -EACCES;  // Read-only
        }
        return 0;
    }
    
    // Проверяем безопасность пути
    if (!is_path_safe(path)) {
        return -EACCES;
    }
    
    pthread_mutex_lock(&data->mutex);
    
    // Ищем файл
    struct archive_file *file = find_file(data, path);
    
    if (!file) {
        pthread_mutex_unlock(&data->mutex);
        return -ENOENT;
    }
    
    // Проверяем права доступа
    if (mask & W_OK) {
        pthread_mutex_unlock(&data->mutex);
        return -EACCES;  // Read-only файловая система
    }
    
    if (mask & X_OK) {
        // Для директорий X_OK означает право на поиск
        if (!file->is_dir) {
            // Для файлов проверяем бит выполнения
            if ((file->mode & (S_IXUSR | S_IXGRP | S_IXOTH)) == 0) {
                pthread_mutex_unlock(&data->mutex);
                return -EACCES;
            }
        }
    }
    
    pthread_mutex_unlock(&data->mutex);
    return 0;
}
