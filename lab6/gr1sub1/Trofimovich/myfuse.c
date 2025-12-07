#define FUSE_USE_VERSION 30
#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include "operations.h"

char base_path[PATH_MAX];
int mode_passthrough = 0;
int mode_rot13 = 0;
int mode_upper = 0;

extern struct fuse_operations ops;

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage:\n");
        printf("  %s <source> <mountpoint> --passthrough\n", argv[0]);
        printf("  %s <source> <mountpoint> --rot13\n", argv[0]);
        printf("  %s <source> <mountpoint> --upper\n", argv[0]);
        return 1;
    }

    char *source = argv[1];
    char *mountpoint = argv[2];

    if (!realpath(source, base_path)) {
        perror("realpath");
        return 1;
    }

    // find mode flag
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--passthrough") == 0)
            mode_passthrough = 1;
        else if (strcmp(argv[i], "--rot13") == 0)
            mode_rot13 = 1;
        else if (strcmp(argv[i], "--upper") == 0)
            mode_upper = 1;
    }

    if (!mode_passthrough && !mode_rot13 && !mode_upper) {
        printf("Error: no valid mode specified (--passthrough, --rot13, --upper)\n");
        return 1;
    }

    // Build clean argv for FUSE: program, mountpoint, -f
    char *fuse_argv[4];
    fuse_argv[0] = argv[0];
    fuse_argv[1] = mountpoint;
    fuse_argv[2] = "-f";
    fuse_argv[3] = NULL;

    return fuse_main(3, fuse_argv, &ops, NULL);
}
