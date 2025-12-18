#include "operations.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <fuse.h>  // Добавлено для fuse_main

/* Структура с указателями на операции FUSE */
static struct fuse_operations passthrough_oper = {
    .getattr    = passthrough_getattr,
    .readdir    = passthrough_readdir,
    .open       = passthrough_open,
    .read       = passthrough_read,
    .write      = passthrough_write,
    .create     = passthrough_create,
    .unlink     = passthrough_unlink,
    .mkdir      = passthrough_mkdir,
    .rmdir      = passthrough_rmdir,
    .rename     = passthrough_rename,
    .chmod      = passthrough_chmod,
    .chown      = passthrough_chown,
    .truncate   = passthrough_truncate,
    .utimens    = passthrough_utimens,
    .statfs     = passthrough_statfs,
    .release    = passthrough_release,
    .flush      = passthrough_flush,
    .fsync      = passthrough_fsync,
};

int main(int argc, char *argv[]) {
    /* Настройка обработчиков сигналов */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Проверка аргументов */
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mount_point> [fuse_options]\n", argv[0]);
        fprintf(stderr, "\nOptions:\n");
        fprintf(stderr, "  -f  foreground mode\n");
        fprintf(stderr, "  -d  debug mode\n");
        fprintf(stderr, "  -s  single threaded mode\n");
        fprintf(stderr, "\nExample: %s /tmp/source /mnt/fuse -f\n", argv[0]);
        return 1;
    }
    
    /* Получение абсолютного пути к базовой директории */
    base_path = realpath(argv[1], NULL);
    if (base_path == NULL) {
        perror("realpath");
        fprintf(stderr, "Error: Cannot resolve source directory '%s'\n", argv[1]);
        return 1;
    }
    
    /* Проверка существования и доступности базовой директории */
    struct stat st;
    if (stat(base_path, &st) != 0) {
        perror("stat");
        fprintf(stderr, "Error: Cannot access source directory '%s'\n", base_path);
        free(base_path);
        return 1;
    }
    
    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: Source path '%s' is not a directory\n", base_path);
        free(base_path);
        return 1;
    }
    
    /* Добавляем '/' в конец, если его нет */
    size_t len = strlen(base_path);
    if (base_path[len - 1] != '/') {
        char *new_path = realloc(base_path, len + 2);
        if (new_path == NULL) {
            perror("realloc");
            free(base_path);
            return 1;
        }
        base_path = new_path;
        strcat(base_path, "/");
    }
    
    /* Подготовка аргументов для FUSE */
    fprintf(stderr, "Mounting directory: %s\n", base_path);
    fprintf(stderr, "Mount point: %s\n", argv[2]);
    fprintf(stderr, "\nTo unmount: fusermount -u %s\n", argv[2]);
    fprintf(stderr, "Logs will be written to stderr\n\n");
    
    /* Перемещаем аргументы для FUSE */
    int fuse_argc = argc - 1;
    char **fuse_argv = malloc((fuse_argc + 1) * sizeof(char*));
    if (!fuse_argv) {
        perror("malloc");
        free(base_path);
        return 1;
    }
    
    fuse_argv[0] = argv[0];
    for (int i = 2; i < argc; i++) {
        fuse_argv[i-1] = argv[i];
    }
    fuse_argv[fuse_argc] = NULL; 
    /* Запуск FUSE */
    int ret = fuse_main(fuse_argc, fuse_argv, &passthrough_oper, NULL);
    
    /* Очистка */
    free(fuse_argv);
    free(base_path);
    
    return ret;
}
