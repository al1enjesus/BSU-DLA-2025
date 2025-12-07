/*
 * myfuse.c
 *
 * Точка входа. Разбирает аргументы:
 *   ./myfuse <source_dir> <mount_point> [fuse options...] [--mode=passthrough|rot13|upper]
 *
 * Передаёт управление FUSE.
 */

#define FUSE_USE_VERSION 31

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "operations.h"

/* extern callbacks implemented in operations.c */
extern struct fuse_operations passthrough_op;

/* We will use a single static struct and assign the functions implemented in operations.c */
static struct fuse_operations fs_oper = {
    .getattr = fs_getattr,
    .readdir = fs_readdir,
    .open    = fs_open,
    .read    = fs_read,
    .write   = fs_write,
    .create  = fs_create,
    .unlink  = fs_unlink,
    .mkdir   = fs_mkdir,
    .rmdir   = fs_rmdir,
    .destroy = fs_destroy
};

/* Parse --mode option */
static fs_mode_t parse_mode(const char *arg) {
    if (!arg) return MODE_PASSTHROUGH;
    if (strcmp(arg, "rot13") == 0) return MODE_ROT13;
    if (strcmp(arg, "upper") == 0) return MODE_UPPER;
    return MODE_PASSTHROUGH;
}

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s <source_dir> <mount_point> [fuse_options] [--mode=passthrough|rot13|upper]\n", prog);
    fprintf(stderr, "Example: %s /tmp/source /mnt/fuse -f --mode=rot13\n", prog);
}

/* Extract mode from argv if present; returns new argc/argv suitable for fuse_main (removes --mode arg) */
static int extract_mode_arg(int argc, char **argv, fs_mode_t *mode) {
    *mode = MODE_PASSTHROUGH;
    int write_idx = 0;
    for (int i = 0; i < argc; ++i) {
        if (strncmp(argv[i], "--mode=", 7) == 0) {
            const char *m = argv[i] + 7;
            *mode = parse_mode(m);
        } else {
            argv[write_idx++] = argv[i];
        }
    }
    return write_idx;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    fs_mode_t mode = MODE_PASSTHROUGH;
    
    int fuse_argc = argc - 1;
    char **argv_copy = malloc(sizeof(char*) * fuse_argc);
    const char *base_dir = argv[1];

    argv_copy[0] = argv[0];
    for (int i = 2; i < argc; i++) argv_copy[i - 1] = argv[i];
    int new_argc = extract_mode_arg(fuse_argc, argv_copy, &mode);    

    int rc = fs_init(base_dir, mode);
    if (rc != 0) {
        fprintf(stderr, "Failed to init filesystem base '%s' (error %d)\n", base_dir, rc);
        free(argv_copy);
        return 1;
    }

    /* Informational print */
    const char *mode_name = (mode == MODE_PASSTHROUGH) ? "passthrough" : (mode == MODE_ROT13) ? "rot13" : "upper";
    fprintf(stderr, "Mounting %s (mode=%s). Use 'fusermount -u <mountpoint>' to unmount.\n", base_dir, mode_name);
    
    /* Call fuse_main with possibly modified argv (mode option removed) */
    int ret = fuse_main(new_argc, argv_copy, &fs_oper, NULL);

    free(argv_copy);
    return ret;
}
