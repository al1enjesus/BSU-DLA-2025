/*
 * =================================================================================
 * ROT13 ФАЙЛОВАЯ СИСТЕМА НА FUSE
 * Цель: Прокси-ФС с шифрованием ROT13 "на лету"
 * Автор: Лабораторная работа по системному программированию
 * =================================================================================
 */

/* Версия FUSE API */
#define FUSE_USE_VERSION 31

/*──────────────────────────────────────────────────────────────────────────────────
 * ЗАГОЛОВОЧНЫЕ ФАЙЛЫ
 *─────────────────────────────────────────────────────────────────────────────────*/
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <time.h>
#include <unistd.h>



static char* base_directory = NULL;


static void log_fs_operation(const char* operation,
                             const char* path,
                             int         result)
{
    time_t current_time = time(NULL);
    char   time_buffer[64];
    
    strftime(time_buffer,
             sizeof(time_buffer),
             "%Y-%m-%d %H:%M:%S",
             localtime(&current_time));
    
    fprintf(stderr,
            "[%s] %-12s: %-40s (результат: %d)\n",
            time_buffer,
            operation,
            path,
            result);
}

static void apply_rot13_cipher(char*   buffer,
                               size_t  length)
{
    for (size_t index = 0; index < length; ++index)
    {
        char character = buffer[index];
        
        if (character >= 'a' && character <= 'z')
        {
            buffer[index] = 'a' + ((character - 'a' + 13) % 26);
        }
        else if (character >= 'A' && character <= 'Z')
        {
            buffer[index] = 'A' + ((character - 'A' + 13) % 26);
        }
        /* Остальные символы остаются без изменений */
    }
}


static void resolve_secure_path(char*       full_path,
                                const char* fuse_path)
{
    char target_path[PATH_MAX];
    
    /* Обработка корневой директории */
    if (strcmp(fuse_path, "/") == 0)
    {
        strcpy(target_path, base_directory);
    }
    else
    {
        snprintf(target_path,
                 PATH_MAX,
                 "%s%s",
                 base_directory,
                 fuse_path);
    }

    if (realpath(target_path, full_path) == NULL)
    {
        strncpy(full_path, target_path, PATH_MAX);
        full_path[PATH_MAX - 1] = '\0';
        return;
    }
    

    size_t base_length = strlen(base_directory);
    
    if (strncmp(full_path, base_directory, base_length) != 0)
    {
        fprintf(stderr,
                "ПРЕРЫВАНИЕ: Попытка обхода пути! Запрос: %s\n",
                fuse_path);
        
        strcpy(full_path, base_directory);  /* Возвращаемся в безопасную зону */
    }
}


static int fs_getattr(const char*           path,
                      struct stat*          stat_info,
                      struct fuse_file_info* file_info)
{
    (void)file_info;  /* Неиспользуемый параметр */
    
    char absolute_path[PATH_MAX];
    resolve_secure_path(absolute_path, path);
    
    int operation_result = lstat(absolute_path, stat_info);
    
    if (operation_result == -1)
    {
        log_fs_operation("GETATTR", path, -errno);
        return -errno;
    }
    
    log_fs_operation("GETATTR", path, 0);
    return 0;
}


static int fs_readdir(const char*           path,
                      void*                 buffer,
                      fuse_fill_dir_t       filler,
                      off_t                 offset,
                      struct fuse_file_info* file_info,
                      enum fuse_readdir_flags flags)
{
    (void)offset;
    (void)file_info;
    (void)flags;
    
    char absolute_path[PATH_MAX];
    resolve_secure_path(absolute_path, path);
    
    DIR* directory = opendir(absolute_path);
    
    if (directory == NULL)
    {
        log_fs_operation("READDIR", path, -errno);
        return -errno;
    }
    
    struct dirent* entry;
    int fill_result = 0;
    
    while ((entry = readdir(directory)) != NULL)
    {
        struct stat entry_stat;
        memset(&entry_stat, 0, sizeof(entry_stat));
        
        entry_stat.st_ino  = entry->d_ino;
        entry_stat.st_mode = entry->d_type << 12;
        
        if (filler(buffer,
                   entry->d_name,
                   &entry_stat,
                   0,
                   0))
        {
            fill_result = -ENOMEM;
            break;
        }
    }
    
    closedir(directory);
    log_fs_operation("READDIR", path, fill_result);
    
    return fill_result;
}


static int fs_open(const char*           path,
                   struct fuse_file_info* file_info)
{
    char absolute_path[PATH_MAX];
    resolve_secure_path(absolute_path, path);
    
    int file_descriptor = open(absolute_path, file_info->flags);
    
    if (file_descriptor == -1)
    {
        log_fs_operation("OPEN", path, -errno);
        return -errno;
    }
    
    close(file_descriptor);
    log_fs_operation("OPEN", path, 0);
    
    return 0;
}

static int fs_read(const char*           path,
                   char*                 buffer,
                   size_t                bytes_to_read,
                   off_t                 offset,
                   struct fuse_file_info* file_info)
{
    (void)file_info;
    
    char absolute_path[PATH_MAX];
    resolve_secure_path(absolute_path, path);
    
    int file_descriptor = open(absolute_path, O_RDONLY);
    
    if (file_descriptor == -1)
    {
        log_fs_operation("READ", path, -errno);
        return -errno;
    }
    
    int bytes_read = pread(file_descriptor,
                           buffer,
                           bytes_to_read,
                           offset);
    

    if (bytes_read > 0)
    {
        apply_rot13_cipher(buffer, bytes_read);
    }
    
    if (bytes_read == -1)
    {
        bytes_read = -errno;
    }
    
    close(file_descriptor);
    log_fs_operation("READ", path, bytes_read);
    
    return bytes_read;
}


static int fs_write(const char*           path,
                    const char*           data,
                    size_t                bytes_to_write,
                    off_t                 offset,
                    struct fuse_file_info* file_info)
{
    (void)file_info;
    
    char absolute_path[PATH_MAX];
    resolve_secure_path(absolute_path, path);
    
    int file_descriptor = open(absolute_path, O_WRONLY);
    
    if (file_descriptor == -1)
    {
        log_fs_operation("WRITE", path, -errno);
        return -errno;
    }
    
    /* Выделение буфера для зашифрованных данных */
    char* encrypted_buffer = (char*)malloc(bytes_to_write);
    
    if (encrypted_buffer == NULL)
    {
        close(file_descriptor);
        log_fs_operation("WRITE", path, -ENOMEM);
        return -ENOMEM;
    }
    

    memcpy(encrypted_buffer, data, bytes_to_write);
    apply_rot13_cipher(encrypted_buffer, bytes_to_write);
    
    int bytes_written = pwrite(file_descriptor,
                               encrypted_buffer,
                               bytes_to_write,
                               offset);
    
    if (bytes_written == -1)
    {
        bytes_written = -errno;
    }
    
    /* Очистка ресурсов */
    free(encrypted_buffer);
    close(file_descriptor);
    
    log_fs_operation("WRITE", path, bytes_written);
    
    return bytes_written;
}


static int fs_create(const char*           path,
                     mode_t                permissions,
                     struct fuse_file_info* file_info)
{
    (void)file_info;
    
    char absolute_path[PATH_MAX];
    resolve_secure_path(absolute_path, path);
    
    int file_descriptor = creat(absolute_path, permissions);
    
    if (file_descriptor == -1)
    {
        log_fs_operation("CREATE", path, -errno);
        return -errno;
    }
    
    close(file_descriptor);
    log_fs_operation("CREATE", path, 0);
    
    return 0;
}


static int fs_unlink(const char* path)
{
    char absolute_path[PATH_MAX];
    resolve_secure_path(absolute_path, path);
    
    int operation_result = unlink(absolute_path);
    
    if (operation_result == -1)
    {
        log_fs_operation("UNLINK", path, -errno);
        return -errno;
    }
    
    log_fs_operation("UNLINK", path, 0);
    return 0;
}


static int fs_mkdir(const char* path,
                    mode_t      permissions)
{
    char absolute_path[PATH_MAX];
    resolve_secure_path(absolute_path, path);
    
    int operation_result = mkdir(absolute_path, permissions);
    
    if (operation_result == -1)
    {
        log_fs_operation("MKDIR", path, -errno);
        return -errno;
    }
    
    log_fs_operation("MKDIR", path, 0);
    return 0;
}


static int fs_rmdir(const char* path)
{
    char absolute_path[PATH_MAX];
    resolve_secure_path(absolute_path, path);
    
    int operation_result = rmdir(absolute_path);
    
    if (operation_result == -1)
    {
        log_fs_operation("RMDIR", path, -errno);
        return -errno;
    }
    
    log_fs_operation("RMDIR", path, 0);
    return 0;
}


static int fs_truncate(const char*           path,
                       off_t                 new_size,
                       struct fuse_file_info* file_info)
{
    (void)file_info;
    
    char absolute_path[PATH_MAX];
    resolve_secure_path(absolute_path, path);
    
    int operation_result = truncate(absolute_path, new_size);
    
    if (operation_result == -1)
    {
        log_fs_operation("TRUNCATE", path, -errno);
        return -errno;
    }
    
    log_fs_operation("TRUNCATE", path, 0);
    return 0;
}


static int fs_chmod(const char*           path,
                    mode_t                new_permissions,
                    struct fuse_file_info* file_info)
{
    (void)file_info;
    
    char absolute_path[PATH_MAX];
    resolve_secure_path(absolute_path, path);
    
    int operation_result = chmod(absolute_path, new_permissions);
    
    if (operation_result == -1)
    {
        log_fs_operation("CHMOD", path, -errno);
        return -errno;
    }
    
    log_fs_operation("CHMOD", path, 0);
    return 0;
}


static int fs_chown(const char*           path,
                    uid_t                 new_user_id,
                    gid_t                 new_group_id,
                    struct fuse_file_info* file_info)
{
    (void)file_info;
    
    char absolute_path[PATH_MAX];
    resolve_secure_path(absolute_path, path);
    
    int operation_result = lchown(absolute_path,
                                  new_user_id,
                                  new_group_id);
    
    if (operation_result == -1)
    {
        log_fs_operation("CHOWN", path, -errno);
        return -errno;
    }
    
    log_fs_operation("CHOWN", path, 0);
    return 0;
}


static struct fuse_operations fuse_operations_table = {
    .getattr  = fs_getattr,
    .readdir  = fs_readdir,
    .open     = fs_open,
    .read     = fs_read,
    .write    = fs_write,
    .create   = fs_create,
    .unlink   = fs_unlink,
    .mkdir    = fs_mkdir,
    .rmdir    = fs_rmdir,
    .truncate = fs_truncate,
    .chmod    = fs_chmod,
    .chown    = fs_chown,
};


int main(int   argument_count,
         char* argument_values[])
{
  
    if (argument_count < 3)
    {
        fprintf(stderr,
                "СИНТАКСИС: %s <исходная_директория> <точка_монтирования> [опции_fuse]\n",
                argument_values[0]);
        return EXIT_FAILURE;
    }
    

    base_directory = realpath(argument_values[1], NULL);
    
    if (base_directory == NULL)
    {
        perror("ОШИБКА realpath");
        return EXIT_FAILURE;
    }
    
    fprintf(stderr,
            "МОНТИРОВАНИЕ: Источник: %s -> Точка монтирования: %s\n",
            base_directory,
            argument_values[2]);
    
   
    int fuse_argument_count = argument_count - 1;
    char** fuse_arguments = malloc(sizeof(char*) * fuse_argument_count);
    
    if (fuse_arguments == NULL)
    {
        perror("ОШИБКА выделения памяти");
        free(base_directory);
        return EXIT_FAILURE;
    }
    
    /* Копирование аргументов, пропуская исходную директорию */
    fuse_arguments[0] = argument_values[0];
    
    for (int index = 2; index < argument_count; ++index)
    {
        fuse_arguments[index - 1] = argument_values[index];
    }
    

    int fuse_result = fuse_main(fuse_argument_count,
                                fuse_arguments,
                                &fuse_operations_table,
                                NULL);
    

    free(fuse_arguments);
    free(base_directory);
    
    return fuse_result;
}

