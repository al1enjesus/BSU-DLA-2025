#define _GNU_SOURCE
#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "operations.h"

static const char *source_dir = NULL;

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mountpoint> [fuse options]\n", argv[0]);
        return 1;
    }

    source_dir = realpath(argv[1], NULL);
    if (!source_dir) {
        perror("realpath for source_dir");
        return 1;
    }

    int new_argc = argc - 1;
    char **new_argv = malloc((new_argc + 1) * sizeof(char*));
    if (!new_argv) {
        perror("malloc");
        free((void*)source_dir);
        return 1;
    }

    new_argv[0] = argv[0];
    for (int i = 2; i < argc; ++i)
        new_argv[i - 1] = argv[i];
    new_argv[new_argc] = NULL;

    struct fuse_operations ops = {0};
    fill_operations(&ops);

    int ret = fuse_main(new_argc, new_argv, &ops, (void*)source_dir);

    free(new_argv);
    free((void*)source_dir);
    return ret;
}