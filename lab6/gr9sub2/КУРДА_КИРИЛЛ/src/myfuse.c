#define FUSE_USE_VERSION 30
#define _GNU_SOURCE
#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <limits.h>
#include <ctype.h>

/* ----- Configuration ----- */
static const size_t TRANS_BUF_SIZE = 16 * 1024; /* буфер для трансформаций */

/* ----- Global state ----- */
static char *g_root = NULL;
enum MODE { MODE_PASSTHROUGH = 0, MODE_ROT13 = 1, MODE_UPPER = 2 };
static enum MODE g_mode = MODE_PASSTHROUGH;

/* ----- Helpers ----- */

static void logop(const char *op, const char *path, int res)
{
    time_t t = time(NULL);
    char ts[64];
    strftime(ts, sizeof(ts), "%F %T", localtime(&t));
    fprintf(stderr, "[%s] %s: %s (result: %d)\n", ts, op, path ? path : "(null)", res);
    fflush(stderr);
}

/* Prevent path traversal: reject any ".." component */
static int check_path_ok(const char *path)
{
    if (!path) return -EACCES;
    if (strstr(path, "..")) return -EACCES;
    return 0;
}

/* build full path: root + path; path comes like "/foo/bar" */
static void build_fullpath(const char *path, char *out, size_t outlen)
{
    if (!g_root) {
        snprintf(out, outlen, "%s", path);
        return;
    }
    size_t rlen = strlen(g_root);
    if (rlen > 0 && g_root[rlen-1] == '/')
        snprintf(out, outlen, "%s%s", g_root, path[0]=='/' ? path+1 : path);
    else
        snprintf(out, outlen, "%s/%s", g_root, path[0]=='/' ? path+1 : path);
}

/* ROT13 in-place on buffer of length len */
static void rot13_buf(char *buf, ssize_t len)
{
    for (ssize_t i = 0; i < len; ++i) {
        char c = buf[i];
        if (c >= 'a' && c <= 'z') buf[i] = 'a' + ((c - 'a' + 13) % 26);
        else if (c >= 'A' && c <= 'Z') buf[i] = 'A' + ((c - 'A' + 13) % 26);
    }
}

/* Uppercase in-place */
static void upper_buf(char *buf, ssize_t len)
{
    for (ssize_t i = 0; i < len; ++i) buf[i] = toupper((unsigned char)buf[i]);
}

/* ----- FUSE ops ----- */

static int fs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    (void) fi;
    if (check_path_ok(path) < 0) {
        logop("GETATTR (bad path)", path, -EACCES);
        return -EACCES;
    }

    char full[PATH_MAX];
    build_fullpath(path, full, sizeof(full));

    int res = lstat(full, stbuf);
    if (res == -1) {
        int err = -errno;
        logop("GETATTR", path, err);
        return err;
    }
    logop("GETATTR", path, 0);
    return 0;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags)
{
    (void) offset; (void) fi; (void) flags;

    if (check_path_ok(path) < 0) {
        logop("READDIR (bad path)", path, -EACCES);
        return -EACCES;
    }

    char full[PATH_MAX];
    build_fullpath(path, full, sizeof(full));

    DIR *dp = opendir(full);
    if (!dp) {
        int err = -errno;
        logop("READDIR", path, err);
        return err;
    }

    struct dirent *de;
    /* add . and .. */
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    while ((de = readdir(dp)) != NULL) {
        if (filler(buf, de->d_name, NULL, 0, 0))
            break;
    }
    closedir(dp);
    logop("READDIR", path, 0);
    return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi)
{
    if (check_path_ok(path) < 0) {
        logop("OPEN (bad path)", path, -EACCES);
        return -EACCES;
    }

    char full[PATH_MAX];
    build_fullpath(path, full, sizeof(full));

    int flags = fi->flags;
    int fd = open(full, flags);
    if (fd == -1) {
        int err = -errno;
        logop("OPEN", path, err);
        return err;
    }
    close(fd);
    logop("OPEN", path, 0);
    return 0;
}

static int fs_read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi)
{
    (void) fi;
    if (check_path_ok(path) < 0) {
        logop("READ (bad path)", path, -EACCES);
        return -EACCES;
    }

    char full[PATH_MAX];
    build_fullpath(path, full, sizeof(full));

    int fd = open(full, O_RDONLY);
    if (fd == -1) {
        int err = -errno;
        logop("READ open", path, err);
        return err;
    }

    size_t bufsz = (size > TRANS_BUF_SIZE) ? size : TRANS_BUF_SIZE;
    char *tmp = malloc(bufsz);
    if (!tmp) {
        close(fd);
        logop("READ malloc", path, -ENOMEM);
        return -ENOMEM;
    }

    ssize_t res = pread(fd, tmp, size, offset);
    if (res == -1) {
        int err = -errno;
        free(tmp); close(fd);
        logop("READ pread", path, err);
        return err;
    }

    /* apply transform if needed */
    if (g_mode == MODE_ROT13) {
        rot13_buf(tmp, res);
    } else if (g_mode == MODE_UPPER) {
        upper_buf(tmp, res);
    }

    /* copy to user buffer provided by FUSE */
    memcpy(buf, tmp, res);
    free(tmp);
    close(fd);
    logop("READ", path, (int)res);
    return res;
}

static int fs_write(const char *path, const char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
    (void) fi;
    if (check_path_ok(path) < 0) {
        logop("WRITE (bad path)", path, -EACCES);
        return -EACCES;
    }

    char full[PATH_MAX];
    build_fullpath(path, full, sizeof(full));

    /* apply transform to a local buffer before writing */
    size_t bufsz = (size > TRANS_BUF_SIZE) ? size : TRANS_BUF_SIZE;
    char *tmp = malloc(bufsz);
    if (!tmp) {
        logop("WRITE malloc", path, -ENOMEM);
        return -ENOMEM;
    }
    memcpy(tmp, buf, size);

    if (g_mode == MODE_ROT13) rot13_buf(tmp, size);
    /* MODE_UPPER: keep writes as-is (requirement: uppercase only on read) */

    int fd = open(full, O_WRONLY);
    if (fd == -1) {
        /* try create if it doesn't exist */
        if (errno == ENOENT) {
            fd = open(full, O_WRONLY | O_CREAT, 0644);
        }
        if (fd == -1) {
            int err = -errno;
            free(tmp);
            logop("WRITE open", path, err);
            return err;
        }
    }

    ssize_t res = pwrite(fd, tmp, size, offset);
    if (res == -1) {
        int err = -errno;
        free(tmp); close(fd);
        logop("WRITE pwrite", path, err);
        return err;
    }

    free(tmp);
    close(fd);
    logop("WRITE", path, (int)res);
    return res;
}

static int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    (void) fi;
    if (check_path_ok(path) < 0) {
        logop("CREATE (bad path)", path, -EACCES);
        return -EACCES;
    }

    char full[PATH_MAX];
    build_fullpath(path, full, sizeof(full));
    int fd = creat(full, mode);
    if (fd == -1) {
        int err = -errno;
        logop("CREATE", path, err);
        return err;
    }
    close(fd);
    logop("CREATE", path, 0);
    return 0;
}

static int fs_unlink(const char *path)
{
    if (check_path_ok(path) < 0) {
        logop("UNLINK (bad path)", path, -EACCES);
        return -EACCES;
    }
    char full[PATH_MAX];
    build_fullpath(path, full, sizeof(full));
    if (unlink(full) == -1) {
        int err = -errno;
        logop("UNLINK", path, err);
        return err;
    }
    logop("UNLINK", path, 0);
    return 0;
}

static int fs_mkdir(const char *path, mode_t mode)
{
    if (check_path_ok(path) < 0) {
        logop("MKDIR (bad path)", path, -EACCES);
        return -EACCES;
    }
    char full[PATH_MAX];
    build_fullpath(path, full, sizeof(full));
    if (mkdir(full, mode) == -1) {
        int err = -errno;
        logop("MKDIR", path, err);
        return err;
    }
    logop("MKDIR", path, 0);
    return 0;
}

static int fs_rmdir(const char *path)
{
    if (check_path_ok(path) < 0) {
        logop("RMDIR (bad path)", path, -EACCES);
        return -EACCES;
    }
    char full[PATH_MAX];
    build_fullpath(path, full, sizeof(full));
    if (rmdir(full) == -1) {
        int err = -errno;
        logop("RMDIR", path, err);
        return err;
    }
    logop("RMDIR", path, 0);
    return 0;
}

/* FUSE operations struct */
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
};

/* ----- main ----- */
static void print_usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <source_dir> <mountpoint> [--mode=passthrough|rot13|upper]\n", prog);
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    g_root = realpath(argv[1], NULL);
    if (!g_root) {
        perror("realpath(source_dir)");
        return 1;
    }

    const char *mountpoint = argv[2];

    /* parse optional mode */
    for (int i = 3; i < argc; ++i) {
        if (strncmp(argv[i], "--mode=", 7) == 0) {
            const char *m = argv[i] + 7;
            if (strcmp(m, "passthrough") == 0) g_mode = MODE_PASSTHROUGH;
            else if (strcmp(m, "rot13") == 0) g_mode = MODE_ROT13;
            else if (strcmp(m, "upper") == 0) g_mode = MODE_UPPER;
            else {
                fprintf(stderr, "Unknown mode '%s'\n", m);
                free(g_root);
                return 1;
            }
        }
    }

    fprintf(stderr, "myfuse: root=%s mountpoint=%s mode=%d\n", g_root, mountpoint, g_mode);

    /* --- Правильная подготовка аргументов для libfuse --- */
    {
        /* initialize empty args structure */
        struct fuse_args args = FUSE_ARGS_INIT(0, NULL);

        /* program name */
        fuse_opt_add_arg(&args, argv[0]);

        /* add libfuse options (must come before mountpoint) */
        fuse_opt_add_arg(&args, "-f"); /* foreground so logs appear in stderr */

        /* mountpoint */
        fuse_opt_add_arg(&args, (char *)mountpoint);

        /* call fuse_main with prepared args */
        int ret = fuse_main(args.argc, args.argv, &fs_oper, NULL);

        /* free args if fuse_main returns */
        fuse_opt_free_args(&args);

        free(g_root);
        return ret;
    }
}
