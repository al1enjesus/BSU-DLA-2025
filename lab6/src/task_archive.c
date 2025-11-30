#define FUSE_USE_VERSION 26

#include <fuse.h> 
#include "task_archive.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <archive.h>
#include <archive_entry.h>

#define MAX_ARCHIVE_ENTRIES 1024
#define MAX_ENTRY_NAME 256

// Структура для хранения метаданных файла и его содержимого в памяти.
struct ArchiveMetaRecord {
    char entry_name[MAX_ENTRY_NAME];
    size_t data_size;
    mode_t file_mode;
    void *data_buffer; // Буфер для хранения содержимого файла
};

static struct ArchiveMetaRecord archive_index_data[MAX_ARCHIVE_ENTRIES];
static int index_count = 0;
static const char *archive_file_path = NULL;


/**
 * @brief Поиск записи по пути.
 */
static struct ArchiveMetaRecord *find_record_by_path(const char *fuse_path) {
    if (strcmp(fuse_path, "/") == 0) {
        return (struct ArchiveMetaRecord *)-1; 
    }
    
    // Удаляем ведущий слэш для поиска
    const char *name_in_archive = (fuse_path[0] == '/') ? fuse_path + 1 : fuse_path;

    for (int i = 0; i < index_count; i++) {
        if (strcmp(archive_index_data[i].entry_name, name_in_archive) == 0) {
            return &archive_index_data[i];
        }
    }
    return NULL;
}


/**
 * @brief Инициализация и парсинг tar-файла (с кешированием содержимого).
 */
static int initialize_archive_index(const char *path) {
    struct archive *a;
    struct archive_entry *entry;
    
    // Очистка старых данных и освобождение памяти
    for (int i = 0; i < index_count; i++) {
        free(archive_index_data[i].data_buffer);
        archive_index_data[i].data_buffer = NULL;
    }
    index_count = 0;
    
    a = archive_read_new();
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a); // Поддержка gzip, bzip2 и т.д.

    if (archive_read_open_filename(a, path, 10240) != ARCHIVE_OK) {
        fprintf(stderr, "ArchiveHandler: Failed to open archive %s: %s\n", path, archive_error_string(a));
        archive_read_free(a);
        return -ENOENT;
    }
    
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK && index_count < MAX_ARCHIVE_ENTRIES) {
        const char *pathname = archive_entry_pathname(entry);
        size_t entry_size = (size_t)archive_entry_size(entry);
        
        if (pathname && strlen(pathname) < MAX_ENTRY_NAME && 
            S_ISREG(archive_entry_filetype(entry))) 
        {
            // Убеждаемся, что файл не пустой и является регулярным файлом
            if (entry_size == 0) {
                archive_read_data_skip(a);
                continue;
            }

            struct ArchiveMetaRecord *rec = &archive_index_data[index_count];
            strncpy(rec->entry_name, pathname, MAX_ENTRY_NAME - 1);
            rec->entry_name[MAX_ENTRY_NAME - 1] = '\0';
            
            rec->data_size  = entry_size;
            rec->file_mode  = archive_entry_mode(entry);
            rec->data_buffer = malloc(entry_size);

            if (!rec->data_buffer) {
                fprintf(stderr, "ArchiveHandler: Memory allocation failed for entry %s.\n", pathname);
                archive_read_data_skip(a);
                continue; 
            }
            
            // Читаем данные из архива
            la_ssize_t read_res = archive_read_data(a, rec->data_buffer, entry_size);

            if (read_res != (la_ssize_t)entry_size) {
                 fprintf(stderr, "ArchiveHandler: Failed to read full data for entry %s.\n", pathname);
                 free(rec->data_buffer);
                 archive_read_data_skip(a);
                 continue; // Пропускаем эту запись
            }

            index_count++;
        } else {
             // Пропускаем директории, пустые или слишком длинные записи
             archive_read_data_skip(a);
        }
    }

    archive_read_close(a);
    archive_read_free(a);
    
    archive_file_path = path;
    return 0;
}


/* ---------------- task_archive_get_stat (getattr) ---------------- */
// FUSE 2 signature: no fi
static int task_archive_get_stat(const char *path, struct stat *stat_buffer) {
    memset(stat_buffer, 0, sizeof(struct stat));
    
    struct ArchiveMetaRecord *record = find_record_by_path(path);

    if (record == NULL) {
        return -ENOENT;
    }
    
    if (record == (struct ArchiveMetaRecord *)-1) {
        // Корень
        stat_buffer->st_mode = S_IFDIR | 0555; // <-- Использование S_IFDIR
        stat_buffer->st_nlink = 2;
        return 0;
    }

    // Запись из архива
    stat_buffer->st_mode = record->file_mode;
    stat_buffer->st_nlink = 1;
    stat_buffer->st_size = record->data_size;
    
    // Запрет записи
    stat_buffer->st_mode &= ~0222; 
    
    return 0;
}

/* ---------------- task_archive_list_dir (readdir) ---------------- */
// FUSE 2 signature: 5 аргументов для совместимости с FUSE 2.6
static int task_archive_list_dir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    (void)offset;
    (void)fi; 

    if (strcmp(path, "/") != 0) {
        return -ENOENT;
    }

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    for (int i = 0; i < index_count; i++) {
        struct ArchiveMetaRecord *record = &archive_index_data[i];
        
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_mode = record->file_mode;

        // FUSE 2 filler call (4 аргумента)
        if (filler(buf, record->entry_name, &st, 0)) {
            break;
        }
    }

    return 0;
}

/* ---------------- task_archive_open_file (open) ---------------- */
static int task_archive_open_file(const char *path, struct fuse_file_info *fi) {
    // Архив только для чтения
    if ((fi->flags & O_ACCMODE) != O_RDONLY) {
        return -EACCES; 
    }

    struct ArchiveMetaRecord *record = find_record_by_path(path);
    if (record == NULL || record == (struct ArchiveMetaRecord *)-1 || S_ISDIR(record->file_mode)) {
        return -ENOENT;
    }

    // Сохраняем указатель на запись в fi->fh
    fi->fh = (uint64_t)record;
    
    return 0;
}

/* ---------------- task_archive_read_file (read) ---------------- */
static int task_archive_read_file(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void)path; 

    struct ArchiveMetaRecord *record = (struct ArchiveMetaRecord *)fi->fh;

    if (!record || !record->data_buffer) {
        return -EBADF; 
    }
    
    // Исправлено: сравнение off_t (signed) с size_t (unsigned)
    if ((size_t)offset >= record->data_size) {
        return 0;
    }
    
    size_t available_data = record->data_size - (size_t)offset;
    size_t bytes_to_read = (available_data < size) ? available_data : size;
    
    // Чтение из кешированного буфера
    memcpy(buf, (char*)record->data_buffer + offset, bytes_to_read);

    return (int)bytes_to_read;
}

// Операции для Archive FS (только чтение)
static struct fuse_operations archive_fs_operations = {
    .getattr    = task_archive_get_stat,
    .readdir    = task_archive_list_dir,
    .open       = task_archive_open_file,
    .read       = task_archive_read_file,
    .release    = NULL, 
    .write      = NULL, 
    .create     = NULL, 
    .unlink     = NULL, 
    .mkdir      = NULL, 
    .rmdir      = NULL, 
};

const struct fuse_operations *task_archive_get_operands(const char *archive_path) {
    if (initialize_archive_index(archive_path) != 0) {
        return NULL;
    }
    return &archive_fs_operations;
}