#define _XOPEN_SOURCE 700
#define FUSE_USE_VERSION 26

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fuse.h>
#include <limits.h>

#include "operations.h"

// Структура, связывающая VFS операции с нашими функциями
static struct fuse_operations passthrough_oper = {
    .getattr    = my_getattr,
    .readdir    = my_readdir,
    .open       = my_open,
    .read       = my_read,
    .write      = my_write,
    .create     = my_create,
    .unlink     = my_unlink,
    .mkdir      = my_mkdir,
    .rmdir      = my_rmdir,
    .release    = my_release,
};

int main(int argc, char *argv[]) {
    int res;

    // Проверка аргументов
    if (argc < 3) {
        fprintf(stderr, "Использование: %s <source_dir> <mount_point> [fuse_options]\n", 
                argv[0]);
        fprintf(stderr, "Пример: %s /tmp/source /mnt/fuse -f\n", argv[0]);
        return 1;
    }

    // 1. Получаем абсолютный путь к source_dir
    // realpath выделяет память, которую нужно освободить
    source_dir = realpath(argv[1], NULL);
    if (source_dir == NULL) {
        perror("Ошибка: не удалось получить абсолютный путь для source_dir");
        return 1;
    }
    
    // 2. Гарантируем, что source_dir заканчивается на '/'
    size_t len = strlen(source_dir);
    if (source_dir[len - 1] != '/') {
        // Добавляем 2 байта: один для '/' и один для '\0'
        char *new_source_dir = (char *)realloc(source_dir, len + 2);
        if (new_source_dir == NULL) {
            perror("Ошибка выделения памяти для source_dir");
            free(source_dir);
            return 1;
        }
        source_dir = new_source_dir;
        source_dir[len] = '/';
        source_dir[len+1] = '\0';
    }


    fprintf(stderr, "========================================\n");
    fprintf(stderr, "  FUSE Passthrough FS запущен\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "Source Dir:  %s\n", source_dir);
    fprintf(stderr, "Mount Point: %s\n", argv[2]);
    fprintf(stderr, "========================================\n\n");

    // Запускаем FUSE (передаём argc-1 и argv+1 чтобы пропустить source_dir)
    res = fuse_main(argc - 1, argv + 1, &passthrough_oper, NULL);

    // Освобождаем память
    free(source_dir);

    return res;
}