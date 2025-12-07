#define FUSE_USE_VERSION 35
#include "../include/fuse_fs.h"

void usage(const char *prog) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s <source_dir> <mount_point>          # passthrough + monitoring\n", prog);
    fprintf(stderr, "  %s -t <archive.tar> <mount_point>      # tar read-only\n", prog);
}

int main(int argc, char *argv[]) {
    if (argc < 3) { usage(argv[0]); return 1; }

    config.mode = MODE_PASSTHROUGH;
    config.stats_enabled = 1;

    int i = 1;
    if (strcmp(argv[1], "-t") == 0) {
        if (argc != 4) { usage(argv[0]); return 1; }
        config.mode = MODE_TAR_READONLY;
        strncpy(config.source_path, argv[2], PATH_MAX - 1);
        i = 3;
    } else {
        if (argc != 3) { usage(argv[0]); return 1; }
        strncpy(config.source_path, argv[1], PATH_MAX - 1);
        i = 2;
    }

    const char *mount_point = argv[i];

    if (config.mode == MODE_TAR_READONLY) {
        if (tar_load(config.source_path) != 0) {
            fprintf(stderr, "Failed to load tar archive\n");
            return 1;
        }
    }

    struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
    fuse_opt_add_arg(&args, argv[0]);
    fuse_opt_add_arg(&args, mount_point);

    struct fuse_operations *ops = (config.mode == MODE_TAR_READONLY) ? &tar_ops : &passthrough_ops;

    int res = fuse_main(args.argc, args.argv, ops, NULL);

    if (config.mode == MODE_TAR_READONLY) {
        tar_free(&g_tar);
    }

    return res;
}