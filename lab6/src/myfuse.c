#define _XOPEN_SOURCE 700 // Активирует realpath и многие POSIX-типы
#define FUSE_USE_VERSION 26

// --- 1. Системные и POSIX типы ---
#include <stdio.h>
#include <stdlib.h>     // ОПРЕДЕЛЯЕТ realpath()
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>   // struct stat, косвенно timespec
#include <time.h>       // ОПРЕДЕЛЯЕТ struct timespec

// --- 2. FUSE ---
#include <fuse.h>       // Включается после time.h

#include "operations.h"

// Структура, связывающая системные вызовы VFS с нашими функциями
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
    .release    = my_release, // Важно для закрытия файловых дескрипторов
};

int main(int argc, char *argv[]) {
    int res;
    
    // Проверка наличия обязательных аргументов: <source_dir> <mount_point>
    if (argc < 3) {
        fprintf(stderr, "Использование: %s <source_dir> <mount_point> [fuse_options]\n", argv[0]);
        return 1;
    }

    // 1. Получаем абсолютный путь к базовой директории
    source_dir = realpath(argv[1], NULL);
    if (source_dir == NULL) {
        perror("Ошибка: Не удалось получить абсолютный путь для source_dir");
        return 1;
    }
    
    fprintf(stderr, "--- FUSE Passthrough FS запущен ---\n");
    fprintf(stderr, "Source Dir: %s\n", source_dir);
    fprintf(stderr, "Mount Point: %s\n", argv[2]);
    fprintf(stderr, "-------------------------------------\n");

    // 2. Запускаем FUSE
    // Необходимо передать fuse_main аргументы, начинающиеся с mount_point.
    // Поэтому используем argc - 1 и argv + 1.
    res = fuse_main(argc - 1, argv + 1, &passthrough_oper, NULL);

    // 3. Очистка
    free(source_dir);
    return res;
}