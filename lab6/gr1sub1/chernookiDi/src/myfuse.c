#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "operations.h"

/*
 * fs_ops
 * ------
 * Таблица колбеков, передаваемая в libfuse. Каждое поле указывает на
 * соответствующую реализацию операции в `operations.c`.
 */
static struct fuse_operations fs_ops = {
    .getattr = fs_getattr,
    .readdir = fs_readdir,
    .open = fs_open,
    .read = fs_read,
    .write = fs_write,
    .create = fs_create,
    .unlink = fs_unlink,
    .mkdir = fs_mkdir,
    .rmdir = fs_rmdir,
    .release = fs_release,
    .utimens = fs_utimens,
    .access = fs_access,
    .flush = fs_flush,
};

/*
 * print_usage
 * -----------
 * Печатает краткую инструкцию по запуску программы.
 */
static void print_usage() {
    fprintf(stderr, "Usage: myfuse <source_dir> <mountpoint> [-m passthrough|rot13|uppercase] [-f -d]\n");
}

/*
 * main
 * ----
 * Точка входа программы. Параметры:
 * - argv[1]: исходная (source) директория, которую будем зеркалировать.
 * - argv[2]: точка монтирования (mountpoint) — пробрасывается в fuse_main.
 * Дополнительный параметр: `-m <mode>` для выбора режима: `rot13` или `uppercase`.
 * Все остальные опции (например -f, -d) передаются напрямую в libfuse.
 */
int main(int argc, char *argv[]) {
    if (argc < 3) {
        print_usage();
        return 1;
    }
    /* Сохраняем исходную директорию */
    strncpy(g_config.root, argv[1], PATH_MAX_LEN-1);
    g_config.root[PATH_MAX_LEN-1] = 0;

    /* По умолчанию режим passthrough */
    g_config.mode = MODE_PASSTHROUGH;

    /* Строим аргументы для fuse_main: берём argv[0] и все опции кроме argv[1]
       (source). Опция -m обрабатывается отдельно и не передаётся в libfuse. */
    int fusargc = argc - 1; // пока оценка длины
    char **fusargv = malloc(sizeof(char*) * (argc+1));
    if (!fusargv) {
        fprintf(stderr, "allocation failed\n");
        return 1;
    }
    fusargv[0] = argv[0];
    int idx = 1;
    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "-m") == 0 && i+1 < argc) {
            if (strcmp(argv[i+1], "rot13") == 0) g_config.mode = MODE_ROT13;
            else if (strcmp(argv[i+1], "uppercase") == 0) g_config.mode = MODE_UPPERCASE;
            else g_config.mode = MODE_PASSTHROUGH;
            i++; /* пропускаем параметр режима */
            continue;
        }
        fusargv[idx++] = argv[i];
    }
    fusargc = idx;

    /* Передаём управление в libfuse (выполнение не возвращается, пока FS смонтирован). */
    int res = fuse_main(fusargc, fusargv, &fs_ops, NULL);
    free(fusargv);
    return res;
}
