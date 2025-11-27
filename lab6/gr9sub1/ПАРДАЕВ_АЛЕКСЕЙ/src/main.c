#define FUSE_USE_VERSION 35
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fuse3/fuse.h>

#include "passthrough.h"
#include "archive_fs.h"
#include "monitoring_fs.h"

static struct fuse_operations *ops;

static void print_usage(const char *prog)
{
    fprintf(stderr,
        "Usage:\n"
        "  %s passthrough <src_dir> <mountpoint> [fuse options]\n"
        "  %s archive <file.tar> <mountpoint> [fuse options]\n"
        "  %s monitor <src_dir> <mountpoint> [fuse options]\n",
        prog, prog, prog);
}

int main(int argc, char *argv[])
{
    if (argc < 4) {
        print_usage(argv[0]);
        return 1;
    }

    const char *mode = argv[1];
    const char *source = argv[2];
    const char *mountpoint = argv[3];

    /* Выбор файловой системы */
    if (strcmp(mode, "passthrough") == 0) {
        ops = get_passthrough_ops(source);
    } else if (strcmp(mode, "archive") == 0) {
        ops = get_archive_ops(source);
    } else if (strcmp(mode, "monitor") == 0) {
        ops = get_monitor_ops(source);
    } else {
        fprintf(stderr, "Unknown mode: %s\n", mode);
        print_usage(argv[0]);
        return 1;
    }

    /*
     * Формируем массив аргументов для fuse_main:
     *
     * new_argv[0] = program name
     * new_argv[1] = mountpoint
     * new_argv[2..] = все FUSE-опции, которые шли после mountpoint
     */

    int new_argc = 0;
    char **new_argv = calloc(argc, sizeof(char*));
    if (!new_argv) {
        perror("calloc");
        return 1;
    }

    new_argv[new_argc++] = argv[0];        // имя программы
    new_argv[new_argc++] = (char*)mountpoint;

    /* копируем все опции начиная с argv[4] */
    for (int i = 4; i < argc; i++) {
        new_argv[new_argc++] = argv[i];
    }

    new_argv[new_argc] = NULL;

    /* Запуск FUSE */
    int ret = fuse_main(new_argc, new_argv, ops, NULL);

    free(new_argv);
    return ret;
}
