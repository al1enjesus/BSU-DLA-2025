#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>
#include "archive_ops.h"

static struct fuse_operations archive_oper = {
    .init       = archive_init,
    .destroy    = archive_destroy,
    .getattr    = archive_getattr,
    .readdir    = archive_readdir,
    .open       = archive_open,
    .read       = archive_read,
    .statfs     = archive_statfs,
    .access     = archive_access,
};

static void usage(const char *progname) {
    fprintf(stderr, "Archive FUSE Filesystem v1.0\n");
    fprintf(stderr, "Usage: %s <archive.tar> <mountpoint> [options]\n\n", progname);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "    -f   foreground mode\n");
    fprintf(stderr, "    -d   debug mode (implies -f)\n");
    fprintf(stderr, "    -s   single threaded\n\n");
    fprintf(stderr, "Example:\n");
    fprintf(stderr, "    %s archive.tar /mnt/archive -f\n", progname);
    fprintf(stderr, "    %s archive.tar /mnt/archive -d\n", progname);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        usage(argv[0]);
        return 1;
    }
    
    // Согласно требованию: ./archive_fs /path/to/archive.tar /mnt/archive
    // argv[1] = архив, argv[2] = точка монтирования
    
    // Проверяем, что архив существует и доступен для чтения
    if (access(argv[1], R_OK) != 0) {
        fprintf(stderr, "Error: Cannot access archive '%s': %s\n", 
                argv[1], strerror(errno));
        return 1;
    }
    
    // Получаем абсолютный путь к архиву
    char *archive_path = realpath(argv[1], NULL);
    if (!archive_path) {
        fprintf(stderr, "Error: Cannot resolve path to archive '%s': %s\n", 
                argv[1], strerror(errno));
        return 1;
    }
    
    // Проверяем, что это файл (не директория)
    struct stat st;
    if (stat(archive_path, &st) != 0) {
        fprintf(stderr, "Error: Cannot stat archive '%s': %s\n", 
                archive_path, strerror(errno));
        free(archive_path);
        return 1;
    }
    
    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "Error: Not a regular file: %s\n", archive_path);
        free(archive_path);
        return 1;
    }
    
    // Проверяем точку монтирования
    if (access(argv[2], F_OK) != 0) {
        fprintf(stderr, "Warning: Mount point '%s' does not exist\n", argv[2]);
        fprintf(stderr, "Please create it first: mkdir -p %s\n", argv[2]);
        free(archive_path);
        return 1;
    }
    
    // Проверяем, что точка монтирования - директория
    if (stat(argv[2], &st) != 0) {
        fprintf(stderr, "Error: Cannot stat mount point '%s': %s\n", 
                argv[2], strerror(errno));
        free(archive_path);
        return 1;
    }
    
    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: Mount point '%s' is not a directory\n", argv[2]);
        free(archive_path);
        return 1;
    }
    
    // Создаем структуру данных для архива
    struct archive_data *data = malloc(sizeof(struct archive_data));
    if (!data) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(archive_path);
        return 1;
    }
    
    memset(data, 0, sizeof(struct archive_data));
    data->archive_path = archive_path;
    data->archive_size = st.st_size;
    data->fd = -1;
    data->files = NULL;
    data->file_count = 0;
    
    // Логируем информацию о запуске
    fprintf(stderr, "\n=== Archive FUSE Filesystem ===\n");
    fprintf(stderr, "Archive file:      %s\n", data->archive_path);
    fprintf(stderr, "Archive size:      %ld bytes\n", data->archive_size);
    fprintf(stderr, "Mount point:       %s\n", argv[2]);
    fprintf(stderr, "Process ID:        %d\n", getpid());
    fprintf(stderr, "User ID:           %d\n", getuid());
    fprintf(stderr, "Group ID:          %d\n", getgid());
    fprintf(stderr, "\nTo unmount:\n");
    fprintf(stderr, "  fusermount -u %s\n", argv[2]);
    fprintf(stderr, "  OR\n");
    fprintf(stderr, "  kill -INT %d\n", getpid());
    fprintf(stderr, "===============================\n\n");
    
    // Подготавливаем аргументы для FUSE
    // Убираем путь к архиву из аргументов, он будет доступен через private_data
    int fuse_argc = argc - 1;
    char **fuse_argv = malloc((fuse_argc + 1) * sizeof(char*));
    if (!fuse_argv) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(archive_path);
        free(data);
        return 1;
    }
    
    // Первый аргумент - имя программы
    fuse_argv[0] = argv[0];
    
    // Второй аргумент - точка монтирования
    fuse_argv[1] = argv[2];
    
    // Копируем оставшиеся аргументы (опции)
    for (int i = 3; i < argc; i++) {
        fuse_argv[i-1] = argv[i];
    }
    fuse_argv[fuse_argc] = NULL;
    
    // Если включен debug mode, добавляем -f
    int has_debug = 0;
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            has_debug = 1;
            break;
        }
    }
    
    if (has_debug && fuse_argc >= 2) {
        // Проверяем, нет ли уже -f
        int has_f = 0;
        for (int i = 2; i < fuse_argc; i++) {
            if (strcmp(fuse_argv[i], "-f") == 0) {
                has_f = 1;
                break;
            }
        }
        if (!has_f) {
            // Добавляем -f в конец
            char **new_argv = realloc(fuse_argv, (fuse_argc + 2) * sizeof(char*));
            if (new_argv) {
                fuse_argv = new_argv;
                fuse_argv[fuse_argc] = "-f";
                fuse_argv[fuse_argc + 1] = NULL;
                fuse_argc++;
            }
        }
    }
    
    fprintf(stderr, "Starting FUSE with %d arguments:\n", fuse_argc);
    for (int i = 0; i < fuse_argc; i++) {
        fprintf(stderr, "  argv[%d] = %s\n", i, fuse_argv[i]);
    }
    fprintf(stderr, "\n");
    
    // Запускаем FUSE
    int ret = fuse_main(fuse_argc, fuse_argv, &archive_oper, data);
    
    // Очищаем ресурсы
    free(fuse_argv);
    
    return ret;
}
