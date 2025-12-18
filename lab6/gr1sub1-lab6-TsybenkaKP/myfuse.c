/* myfuse.c
 * FUSE FS for Lab6:
 * modes: passthrough | rot13 | uppercase
 *
 * Build: see Makefile
 *
 * Usage examples:
 *   ./myfuse /tmp/source /tmp/mount --mode=passthrough -f
 *   ./myfuse /tmp/source /tmp/mount --mode=rot13 -f
 *   ./myfuse /tmp/source /tmp/mount --mode=uppercase -f
 */

#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <limits.h>

/* globals set from main */
static char *g_base = NULL;      /* realpath of source dir, no trailing slash */
static enum {MODE_PASSTHROUGH, MODE_ROT13, MODE_UPPER} g_mode = MODE_PASSTHROUGH;

/* helper: timestamped logging to stderr
 * result: >=0 -> success (bytes or 0), <0 -> -errno
 */
static void log_op(const char *op, const char *path, long result, const char *extra) {
    time_t t = time(NULL);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&t));

    if (result < 0) {
        /* error */
        if (extra)
            fprintf(stderr, "[%s] %s: %s (error=%ld) %s\n", ts, op, path, result, extra);
        else
            fprintf(stderr, "[%s] %s: %s (error=%ld)\n", ts, op, path, result);
    } else {
        /* success */
        if (extra)
            fprintf(stderr, "[%s] %s: %s (result=%ld) %s\n", ts, op, path, result, extra);
        else
            fprintf(stderr, "[%s] %s: %s (result=%ld)\n", ts, op, path, result);
    }
    fflush(stderr);
}

/* simple path traversal protection:
 * reject any path containing ".." or that is absolute beyond mount.
 * This is a conservative check suitable for lab.
 */
static int path_safe(const char *path) {
    if (strstr(path, "..") != NULL) return 0;
    /* should start with '/' or be root; fuse gives leading '/' for paths inside FS */
    if (path[0] != '/') return 0;
    return 1;
}

/* join base + path into out (size must be PATH_MAX) */
static void join_fullpath(char out[PATH_MAX], const char *path) {
    /* base has no trailing slash */
    snprintf(out, PATH_MAX, "%s%s", g_base, path);
}

/* ROT13 inplace */
static void rot13_buf(char *b, ssize_t n) {
    for (ssize_t i = 0; i < n; ++i) {
        char c = b[i];
        if ('a' <= c && c <= 'z') b[i] = 'a' + (c - 'a' + 13) % 26;
        else if ('A' <= c && c <= 'Z') b[i] = 'A' + (c - 'A' + 13) % 26;
    }
}

/* Uppercase inplace (ASCII) */
static void uppercase_buf(char *b, ssize_t n) {
    for (ssize_t i = 0; i < n; ++i) b[i] = toupper((unsigned char)b[i]);
}

/* ------------- FUSE operations ------------- */

/* getattr */
static int my_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    if (!path_safe(path)) {
        log_op("GETATTR", path, -EACCES, "path unsafe");
        return -EACCES;
    }

    char full[PATH_MAX];
    join_fullpath(full, path);

    int res = lstat(full, stbuf);
    if (res == -1) {
        log_op("GETATTR", path, -errno, NULL);
        return -errno;
    }
    log_op("GETATTR", path, 0, NULL);
    return 0;
}

/* readdir (note: has fuse_readdir_flags param on modern libfuse) */
static int my_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags; /* not used */

    if (!path_safe(path)) {
        log_op("READDIR", path, -EACCES, "path unsafe");
        return -EACCES;
    }
    char full[PATH_MAX];
    join_fullpath(full, path);

    DIR *dp = opendir(full);
    if (!dp) {
        log_op("READDIR", path, -errno, NULL);
        return -errno;
    }

    struct dirent *de;
    /* add . and .. */
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    while ((de = readdir(dp)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        filler(buf, de->d_name, NULL, 0, 0);
    }
    closedir(dp);
    log_op("READDIR", path, 0, NULL);
    return 0;
}

/* open: just check accessibility */
static int my_open(const char *path, struct fuse_file_info *fi) {
    if (!path_safe(path)) {
        log_op("OPEN", path, -EACCES, "path unsafe");
        return -EACCES;
    }
    char full[PATH_MAX];
    join_fullpath(full, path);

    int fd = open(full, fi->flags);
    if (fd == -1) {
        log_op("OPEN", path, -errno, NULL);
        return -errno;
    }
    close(fd);
    log_op("OPEN", path, 0, NULL);
    return 0;
}

/* read */
static int my_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void) fi;
    if (!path_safe(path)) {
        log_op("READ", path, -EACCES, "path unsafe");
        return -EACCES;
    }
    char full[PATH_MAX];
    join_fullpath(full, path);

    int fd = open(full, O_RDONLY);
    if (fd == -1) {
        log_op("READ", path, -errno, NULL);
        return -errno;
    }

    ssize_t res = pread(fd, buf, size, offset);
    if (res == -1) {
        int e = -errno;
        close(fd);
        log_op("READ", path, e, NULL);
        return e;
    }
    close(fd);

    /* apply transformations depending on mode */
    if (g_mode == MODE_ROT13) {
        rot13_buf(buf, res); /* stored on disk is ROT13, we decode for user */
    } else if (g_mode == MODE_UPPER) {
        uppercase_buf(buf, res);
    }

    char extra[64];
    snprintf(extra, sizeof(extra), "%zd bytes at %ld", res, offset);
    log_op("READ", path, (long)res, extra);
    return (int)res;
}

/* write */
static int my_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void) fi;
    if (!path_safe(path)) {
        log_op("WRITE", path, -EACCES, "path unsafe");
        return -EACCES;
    }
    char full[PATH_MAX];
    join_fullpath(full, path);

    /* For ROT13 we must transform before writing */
    char *tmpbuf = NULL;
    if (g_mode == MODE_ROT13) {
        tmpbuf = malloc(size);
        if (!tmpbuf) return -ENOMEM;
        memcpy(tmpbuf, buf, size);
        rot13_buf(tmpbuf, size); /* encrypt before writing */
    }

    int fd = open(full, O_WRONLY);
    if (fd == -1) {
        if (tmpbuf) free(tmpbuf);
        log_op("WRITE", path, -errno, NULL);
        return -errno;
    }

    const char *usebuf = (g_mode == MODE_ROT13) ? tmpbuf : buf;
    ssize_t res = pwrite(fd, usebuf, size, offset);
    if (res == -1) {
        int e = -errno;
        close(fd);
        if (tmpbuf) free(tmpbuf);
        log_op("WRITE", path, e, NULL);
        return e;
    }
    close(fd);
    if (tmpbuf) free(tmpbuf);

    char extra[64];
    snprintf(extra, sizeof(extra), "%zd bytes at %ld", res, offset);
    log_op("WRITE", path, (long)res, extra);

    return (int)res;
}

/* create */
static int my_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void) fi;
    if (!path_safe(path)) {
        log_op("CREATE", path, -EACCES, "path unsafe");
        return -EACCES;
    }
    char full[PATH_MAX];
    join_fullpath(full, path);

    int fd = creat(full, mode);
    if (fd == -1) {
        log_op("CREATE", path, -errno, NULL);
        return -errno;
    }
    close(fd);
    log_op("CREATE", path, 0, NULL);
    return 0;
}

/* unlink */
static int my_unlink(const char *path) {
    if (!path_safe(path)) {
        log_op("UNLINK", path, -EACCES, "path unsafe");
        return -EACCES;
    }
    char full[PATH_MAX];
    join_fullpath(full, path);

    int res = unlink(full);
    if (res == -1) {
        log_op("UNLINK", path, -errno, NULL);
        return -errno;
    }
    log_op("UNLINK", path, 0, NULL);
    return 0;
}

/* mkdir */
static int my_mkdir(const char *path, mode_t mode) {
    if (!path_safe(path)) {
        log_op("MKDIR", path, -EACCES, "path unsafe");
        return -EACCES;
    }
    char full[PATH_MAX];
    join_fullpath(full, path);

    int res = mkdir(full, mode);
    if (res == -1) {
        log_op("MKDIR", path, -errno, NULL);
        return -errno;
    }
    log_op("MKDIR", path, 0, NULL);
    return 0;
}

/* rmdir */
static int my_rmdir(const char *path) {
    if (!path_safe(path)) {
        log_op("RMDIR", path, -EACCES, "path unsafe");
        return -EACCES;
    }
    char full[PATH_MAX];
    join_fullpath(full, path);

    int res = rmdir(full);
    if (res == -1) {
        log_op("RMDIR", path, -errno, NULL);
        return -errno;
    }
    log_op("RMDIR", path, 0, NULL);
    return 0;
}

static struct fuse_operations my_ops = {
    .getattr = my_getattr,
    .readdir = my_readdir,
    .open = my_open,
    .read = my_read,
    .write = my_write,
    .create = my_create,
    .unlink = my_unlink,
    .mkdir = my_mkdir,
    .rmdir = my_rmdir
};

/* parse --mode=... */
static void parse_mode_arg(const char *arg) {
    if (strcmp(arg, "--mode=rot13") == 0) g_mode = MODE_ROT13;
    else if (strcmp(arg, "--mode=uppercase") == 0) g_mode = MODE_UPPER;
    else g_mode = MODE_PASSTHROUGH;
}

/* helper: check if path_b is inside path_a (both absolute realpaths) */
static int is_subpath(const char *path_a, const char *path_b) {
    size_t la = strlen(path_a);
    if (la == 0) return 0;
    if (strncmp(path_a, path_b, la) != 0) return 0;
    /* equal prefix, ensure boundary: either exact match or next char is '/' */
    if (path_b[la] == '\0' || path_b[la] == '/') return 1;
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mount_point> [--mode=passthrough|rot13|uppercase] [fuse args]\n", argv[0]);
        return 1;
    }

    /* resolve base dir */
    char *rp = realpath(argv[1], NULL);
    if (!rp) {
        perror("realpath(source_dir)");
        return 1;
    }
    g_base = rp; /* keep to free later */

    /* find optional --mode arg and build new argv for fuse */
    char **fuse_argv = malloc(sizeof(char*) * argc);
    int fuse_argc = 0;
    fuse_argv[fuse_argc++] = argv[0];
    for (int i = 2; i < argc; ++i) {
        if (strncmp(argv[i], "--mode=", 7) == 0) {
            parse_mode_arg(argv[i]);
        } else {
            fuse_argv[fuse_argc++] = argv[i];
        }
    }

    /* sanity: check mountpoint vs source to avoid recursion (source inside mount or vice versa) */
    if (fuse_argc > 1) {
        char *real_mount = realpath(fuse_argv[1], NULL);
        if (real_mount) {
            /* if mount is inside source or source inside mount -> error */
            if (is_subpath(g_base, real_mount)) {
                fprintf(stderr, "ERROR: mountpoint (%s) is inside source (%s). Choose different mountpoint.\n", real_mount, g_base);
                free(real_mount);
                free(fuse_argv);
                free(g_base);
                return 1;
            }
            if (is_subpath(real_mount, g_base)) {
                fprintf(stderr, "ERROR: source (%s) is inside mountpoint (%s). Choose different source.\n", g_base, real_mount);
                free(real_mount);
                free(fuse_argv);
                free(g_base);
                return 1;
            }
            free(real_mount);
        } else {
            /* realpath failed on mount: continue but warn */
            fprintf(stderr, "Warning: realpath failed for mountpoint '%s' (it may not exist yet)\n", fuse_argv[1]);
        }
    }

    fprintf(stderr, "Mounting %s at %s (mode=%s)\n", g_base, (fuse_argc>1?fuse_argv[1]:"<unknown>"),
            (g_mode==MODE_PASSTHROUGH)?"passthrough":(g_mode==MODE_ROT13)?"rot13":"uppercase");

    int ret = fuse_main(fuse_argc, fuse_argv, &my_ops, NULL);

    free(fuse_argv);
    free(g_base);
    return ret;
}

