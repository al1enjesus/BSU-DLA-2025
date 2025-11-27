#include <fuse.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "operations.h"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mount_point> [--mode=passthrough|rot13|upper] [fuse opts]\n", argv[0]);
        return 1;
    }
    const char *mode = "passthrough";
    for (int i = 3; i < argc; ++i) {
        if (strncmp(argv[i], "--mode=", 7) == 0) {
            mode = argv[i] + 7;
        }
    }
    if (init_fuse_environment(argv[1], mode) != 0) {
        fprintf(stderr, "init failed\n");
        return 1;
    }
    int fuse_argc = argc - 1;
char **fuse_argv = malloc(sizeof(char*) * fuse_argc);
if (!fuse_argv) {
    fprintf(stderr, "malloc failed\n");
    cleanup_fuse_environment();
    return -ENOMEM;
}
fuse_argv[0] = argv[0];
for (int i = 2; i < argc; i++) fuse_argv[i-1] = argv[i];
int ret = fuse_main(fuse_argc, fuse_argv, &passthrough_oper, NULL);
free(fuse_argv);
}

