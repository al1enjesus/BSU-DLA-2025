#define _GNU_SOURCE
#include "operations.h"
#include "transforms.h"

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>

char *g_base_path = NULL;

typedef enum {
    MODE_PASSTHROUGH = 0,
    MODE_ROT13,
    MODE_UPPER
} fs_mode_t;
static fs_mode_t g_mode = MODE_PASSTHROUGH;

static int passthrough_getattr(const char *path, struct stat *stbuf);
static int passthrough_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                               off_t offset, struct fuse_file_info *fi);
static int passthrough_open(const char *path, struct fuse_file_info *fi);
static int passthrough_read(const char *path, char *buf, size_t size, off_t offset,
                            struct fuse_file_info *fi);
static int passthrough_write(const char *path, const char *buf, size_t size, off_t offset,
                             struct fuse_file_info *fi);
static int passthrough_create(const char *path, mode_t mode, struct fuse_file_info *fi);
static int passthrough_unlink(const char *path);
static int passthrough_mkdir(const char *path, mode_t mode);
static int passthrough_rmdir(const char *path);
static int passthrough_release(const char *path, struct fuse_file_info *fi);

struct fuse_operations passthrough_oper = {
    .getattr = passthrough_getattr,
    .readdir = passthrough_readdir,
    .open    = passthrough_open,
    .read    = passthrough_read,
    .write   = passthrough_write,
    .create  = passthrough_create,
    .unlink  = passthrough_unlink,
    .mkdir   = passthrough_mkdir,
    .rmdir   = passthrough_rmdir,
    .release = passthrough_release,
};

int init_fuse_environment(const char *source_dir, const char *mode_str) {
    if (!source_dir) return -EINVAL;
    char *rp = realpath(source_dir, NULL);
    if (!rp) return -errno;
    g_base_path = rp;

    if (mode_str) {
        if (strcmp(mode_str, "rot13") == 0) g_mode = MODE_ROT13;
        else if (strcmp(mode_str, "upper") == 0) g_mode = MODE_UPPER;
        else g_mode = MODE_PASSTHROUGH;
    } else {
        g_mode = MODE_PASSTHROUGH;
    }

    fprintf(stderr, "Mounting %s (mode=%s)\n", g_base_path,
            (g_mode == MODE_ROT13) ? "rot13" : (g_mode == MODE_UPPER) ? "upper" : "passthrough");
    return 0;
}

void cleanup_fuse_environment(void) {
    if (g_base_path) {
        free(g_base_path);
        g_base_path = NULL;
    }
}

static int passthrough_getattr(const char *path, struct stat *stbuf) {
    if (!path || !stbuf) return -EINVAL;

    char full[PATH_MAX];
    int rc = build_fullpath(path, full, sizeof(full), 0);
    if (rc < 0) {
        log_operation("GETATTR", path, rc);
        return rc;
    }

    int res = lstat(full, stbuf);
    if (res == -1) {
        int err = -errno;
        log_operation("GETATTR", path, err);
        return err;
    }

    log_operation("GETATTR", path, 0);
    return 0;
}

static int passthrough_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                               off_t offset, struct fuse_file_info *fi) {
    (void) offset;
    (void) fi;

    if (!path) return -EINVAL;
    char full[PATH_MAX];
    int rc = build_fullpath(path, full, sizeof(full), 0);
    if (rc < 0) { log_operation("READDIR", path, rc); return rc; }

    DIR *dp = opendir(full);
    if (!dp) { int err = -errno; log_operation("READDIR", path, err); return err; }

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        char child[PATH_MAX];
        if (snprintf(child, sizeof(child), "%s/%s", full, de->d_name) < (int)sizeof(child)) {
            if (lstat(child, &st) == -1) memset(&st, 0, sizeof(st));
        }
        if (filler(buf, de->d_name, &st, 0)) break;
    }

    closedir(dp);
    log_operation("READDIR", path, 0);
    return 0;
}

static int passthrough_open(const char *path, struct fuse_file_info *fi) {
    if (!path || !fi) return -EINVAL;

    char full[PATH_MAX];
    int rc = build_fullpath(path, full, sizeof(full), 0);
    if (rc < 0) { log_operation("OPEN", path, rc); return rc; }

    /* Open with flags passed from FUSE (fi->flags). */
    int fd = open(full, fi->flags);
    if (fd == -1) {
        int err = -errno;
        log_operation("OPEN", path, err);
        return err;
    }

    fi->fh = (uint64_t)fd;

    log_operation("OPEN", path, 0);
    return 0;
}

static int passthrough_read(const char *path, char *buf, size_t size, off_t offset,
                            struct fuse_file_info *fi) {
    (void) path;
    if (!buf) return -EINVAL;

    int fd = -1;
    if (fi && fi->fh) fd = (int)fi->fh;

    char full[PATH_MAX];
    if (fd == -1) { /* fallback if open didn't set fh */
        int rc = build_fullpath(path, full, sizeof(full), 0);
        if (rc < 0) { log_operation("READ", path, rc); return rc; }
        fd = open(full, O_RDONLY);
        if (fd == -1) { int err = -errno; log_operation("READ", path, err); return err; }
    }

    ssize_t res = pread(fd, buf, size, offset);
    if (res == -1) {
        int err = -errno;
        if (fi && fi->fh) { /* if fd is persistent, don't close here */ }
        else close(fd);
        log_operation("READ", path, err);
        return err;
    }

    if (g_mode == MODE_ROT13 && res > 0) rot13_inplace(buf, (size_t)res);
    else if (g_mode == MODE_UPPER && res > 0) uppercase_inplace(buf, (size_t)res);

    if (!(fi && fi->fh)) close(fd); /* close only if we opened a temporary fd */

    log_operation("READ", path, (int)res);
    return (int)res;
}

static int passthrough_write(const char *path, const char *buf, size_t size, off_t offset,
                             struct fuse_file_info *fi) {
    if (!path || (!buf && size > 0)) return -EINVAL;

    int fd = -1;
    if (fi && fi->fh) fd = (int)fi->fh;

    char full[PATH_MAX];
    if (fd == -1) {
        int rc = build_fullpath(path, full, sizeof(full), 1); /* allow nonexistent for create/write */
        if (rc < 0) { log_operation("WRITE", path, rc); return rc; }
        fd = open(full, O_WRONLY);
        if (fd == -1) { int err = -errno; log_operation("WRITE", path, err); return err; }
    }

    ssize_t wrote = 0;
    if (g_mode == MODE_ROT13 && size > 0) {
        char *tmp = malloc(size);
        if (!tmp) { if (!(fi && fi->fh)) close(fd); log_operation("WRITE", path, -ENOMEM); return -ENOMEM; }
        memcpy(tmp, buf, size);
        rot13_inplace(tmp, size);
        wrote = pwrite(fd, tmp, size, offset);
        free(tmp);
    } else {
        wrote = pwrite(fd, buf, size, offset);
    }

    if (wrote == -1) {
        int err = -errno;
        if (!(fi && fi->fh)) close(fd);
        log_operation("WRITE", path, err);
        return err;
    }

    if (!(fi && fi->fh)) close(fd);

    log_operation("WRITE", path, (int)wrote);
    return (int)wrote;
}

static int passthrough_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void) fi;
    if (!path) return -EINVAL;

    char full[PATH_MAX];
    int rc = build_fullpath(path, full, sizeof(full), 1); /* allow nonexistent */
    if (rc < 0) { log_operation("CREATE", path, rc); return rc; }

    int fd = open(full, O_CREAT | O_EXCL | O_WRONLY, mode);
    if (fd == -1) {
        int err = -errno;
        log_operation("CREATE", path, err);
        return err;
    }
    close(fd);
    log_operation("CREATE", path, 0);
    return 0;
}

static int passthrough_unlink(const char *path) {
    if (!path) return -EINVAL;
    char full[PATH_MAX];
    int rc = build_fullpath(path, full, sizeof(full), 0);
    if (rc < 0) { log_operation("UNLINK", path, rc); return rc; }

    int res = unlink(full);
    if (res == -1) { int err = -errno; log_operation("UNLINK", path, err); return err; }
    log_operation("UNLINK", path, 0);
    return 0;
}

static int passthrough_release(const char *path, struct fuse_file_info *fi) {
    (void) path;
    if (fi && fi->fh) {
        close((int)fi->fh);
        fi->fh = 0;
    }
    log_operation("RELEASE", path, 0);
    return 0;
}

static int passthrough_mkdir(const char *path, mode_t mode) {
    if (!path) return -EINVAL;
    char full[PATH_MAX];
    int rc = build_fullpath(path, full, sizeof(full), 1);
    if (rc < 0) { log_operation("MKDIR", path, rc); return rc; }

    int res = mkdir(full, mode);
    if (res == -1) { int err = -errno; log_operation("MKDIR", path, err); return err; }
    log_operation("MKDIR", path, 0);
    return 0;
}

static int passthrough_rmdir(const char *path) {
    if (!path) return -EINVAL;
    char full[PATH_MAX];
    int rc = build_fullpath(path, full, sizeof(full), 0);
    if (rc < 0) { log_operation("RMDIR", path, rc); return rc; }

    int res = rmdir(full);
    if (res == -1) { int err = -errno; log_operation("RMDIR", path, err); return err; }
    log_operation("RMDIR", path, 0);
    return 0;
}
