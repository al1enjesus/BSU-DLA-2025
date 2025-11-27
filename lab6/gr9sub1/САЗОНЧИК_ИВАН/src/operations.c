#include "operations.h"
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#define DEFAULT_DIRECTORY_SIZE 4096
#define MAX_OPERATION_SIZE (1 * 1024 * 1024 * 1024)

static char* get_full_path_or_error(const char *path, int *error_code) {
    const char *source_dir = (const char*)fuse_get_context()->private_data;
    if (!source_dir) {
        *error_code = -EIO;
        return NULL;
    }
    return build_fullpath_safe(source_dir, path, error_code);
}

static int handle_file_operation(const char *operation, const char *path,
                                int (*file_op)(const char*, int*),
                                const char *success_log) {
    int error_code = 0;
    char *full_path = get_full_path_or_error(path, &error_code);
    if (!full_path) {
        fprintf(stderr, "[%s] %s: %s (error: %s)\n",
                get_timestamp(), operation, path, strerror(-error_code));
        return error_code;
    }

    int result = file_op(full_path, &error_code);

    if (error_code == 0 && success_log) {
        fprintf(stderr, "[%s] %s: %s %s\n",
                get_timestamp(), operation, path, success_log);
    } else if (error_code < 0) {
        fprintf(stderr, "[%s] %s: %s (error: %s)\n",
                get_timestamp(), operation, path, strerror(-error_code));
    }

    free(full_path);
    return error_code;
}

static int check_access_permissions_impl(const char *path, int mode) {
    struct fuse_context *context = fuse_get_context();
    if (!context) {
        return -EACCES;
    }

    struct stat st;
    if (lstat(path, &st) == -1) {
        return -errno;
    }

    uid_t uid = context->uid;
    gid_t gid = context->gid;

    if (uid == 0) {
        return 0;
    }

    if (uid == st.st_uid) {
        if (!(st.st_mode & (mode << 6))) {
            return -EACCES;
        }
    } else if (gid == st.st_gid) {
        if (!(st.st_mode & (mode << 3))) {
            return -EACCES;
        }
    } else {
        if (!(st.st_mode & mode)) {
            return -EACCES;
        }
    }

    return 0;
}

int myfuse_getattr(const char *path, struct stat *stbuf) {
    int error_code = 0;
    char *full_path = get_full_path_or_error(path, &error_code);
    if (!full_path) {
        fprintf(stderr, "[%s] GETATTR: %s (error: %s)\n",
                get_timestamp(), path, strerror(-error_code));
        return error_code;
    }

    int res = lstat(full_path, stbuf);
    if (res == -1) {
        error_code = -errno;
    }

    if (res == 0 && S_ISDIR(stbuf->st_mode)) {
        stbuf->st_size = DEFAULT_DIRECTORY_SIZE;
    }

    fprintf(stderr, "[%s] GETATTR: %s (result: %d)\n",
            get_timestamp(), path, res);

    free(full_path);
    return error_code;
}

int myfuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                   off_t offset, struct fuse_file_info *fi) {
    int error_code = 0;
    char *full_path = get_full_path_or_error(path, &error_code);
    if (!full_path) {
        fprintf(stderr, "[%s] READDIR: %s (error: %s)\n",
                get_timestamp(), path, strerror(-error_code));
        return error_code;
    }

    DIR *dp = opendir(full_path);
    if (!dp) {
        error_code = -errno;
        free(full_path);
        fprintf(stderr, "[%s] READDIR: %s (error: %s)\n",
                get_timestamp(), path, strerror(-error_code));
        return error_code;
    }

    struct dirent *de;
    int file_count = 0;

    while ((de = readdir(dp)) != NULL) {
        if (filler(buf, de->d_name, NULL, 0, 0) != 0) {
            break;
        }
        file_count++;
    }

    closedir(dp);
    free(full_path);

    fprintf(stderr, "[%s] READDIR: %s (%d files)\n",
            get_timestamp(), path, file_count);
    return 0;
}

int myfuse_open(const char *path, struct fuse_file_info *fi) {
    int error_code = 0;
    char *full_path = get_full_path_or_error(path, &error_code);
    if (!full_path) {
        fprintf(stderr, "[%s] OPEN: %s (error: %s)\n",
                get_timestamp(), path, strerror(-error_code));
        return error_code;
    }

    int access_mode = 0;
    if (fi->flags & O_RDONLY) access_mode |= R_OK;
    if (fi->flags & O_WRONLY) access_mode |= W_OK;
    if (fi->flags & O_RDWR) access_mode |= R_OK | W_OK;

    error_code = check_access_permissions_impl(full_path, access_mode);
    if (error_code != 0) {
        free(full_path);
        fprintf(stderr, "[%s] OPEN: %s (error: %s)\n",
                get_timestamp(), path, strerror(-error_code));
        return error_code;
    }

    int fd = open(full_path, fi->flags);
    if (fd == -1) {
        error_code = -errno;
        fprintf(stderr, "[%s] OPEN: %s (error: %s)\n",
                get_timestamp(), path, strerror(-error_code));
    } else {
        close(fd);
        fprintf(stderr, "[%s] OPEN: %s (success)\n", get_timestamp(), path);
    }

    free(full_path);
    return error_code;
}

int myfuse_read(const char *path, char *buf, size_t size, off_t offset,
                struct fuse_file_info *fi) {
    if (size > MAX_OPERATION_SIZE || offset < 0) {
        fprintf(stderr, "[%s] READ: %s (error: invalid size/offset)\n",
                get_timestamp(), path);
        return -EINVAL;
    }

    int error_code = 0;
    char *full_path = get_full_path_or_error(path, &error_code);
    if (!full_path) {
        fprintf(stderr, "[%s] READ: %s (error: %s)\n",
                get_timestamp(), path, strerror(-error_code));
        return error_code;
    }

    int fd = open(full_path, O_RDONLY);
    if (fd == -1) {
        error_code = -errno;
        free(full_path);
        fprintf(stderr, "[%s] READ: %s (error: %s)\n",
                get_timestamp(), path, strerror(-error_code));
        return error_code;
    }

    int res = pread(fd, buf, size, offset);
    if (res == -1) {
        error_code = -errno;
        fprintf(stderr, "[%s] READ: %s (error: %s)\n",
                get_timestamp(), path, strerror(-error_code));
    } else {
        fprintf(stderr, "[%s] READ: %s (%ld bytes at offset %ld)\n",
                get_timestamp(), path, size, offset);
    }

    close(fd);
    free(full_path);
    return error_code == 0 ? res : error_code;
}

int myfuse_write(const char *path, const char *buf, size_t size, off_t offset,
                 struct fuse_file_info *fi) {
    if (size > MAX_OPERATION_SIZE || offset < 0) {
        fprintf(stderr, "[%s] WRITE: %s (error: invalid size/offset)\n",
                get_timestamp(), path);
        return -EINVAL;
    }

    int error_code = 0;
    char *full_path = get_full_path_or_error(path, &error_code);
    if (!full_path) {
        fprintf(stderr, "[%s] WRITE: %s (error: %s)\n",
                get_timestamp(), path, strerror(-error_code));
        return error_code;
    }

    int fd = open(full_path, O_WRONLY);
    if (fd == -1) {
        error_code = -errno;
        free(full_path);
        fprintf(stderr, "[%s] WRITE: %s (error: %s)\n",
                get_timestamp(), path, strerror(-error_code));
        return error_code;
    }

    int res = pwrite(fd, buf, size, offset);
    if (res == -1) {
        error_code = -errno;
        fprintf(stderr, "[%s] WRITE: %s (error: %s)\n",
                get_timestamp(), path, strerror(-error_code));
    } else {
        fprintf(stderr, "[%s] WRITE: %s (%ld bytes at offset %ld)\n",
                get_timestamp(), path, size, offset);
    }

    close(fd);
    free(full_path);
    return error_code == 0 ? res : error_code;
}

int myfuse_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    int error_code = 0;
    char *full_path = get_full_path_or_error(path, &error_code);
    if (!full_path) {
        fprintf(stderr, "[%s] CREATE: %s (error: %s)\n",
                get_timestamp(), path, strerror(-error_code));
        return error_code;
    }

    int fd = open(full_path, fi->flags, mode);
    if (fd == -1) {
        error_code = -errno;
        fprintf(stderr, "[%s] CREATE: %s (error: %s)\n",
                get_timestamp(), path, strerror(-error_code));
    } else {
        close(fd);
        fprintf(stderr, "[%s] CREATE: %s (success)\n", get_timestamp(), path);
    }

    free(full_path);
    return error_code;
}

int myfuse_unlink(const char *path) {
    int error_code = 0;
    char *full_path = get_full_path_or_error(path, &error_code);
    if (!full_path) {
        fprintf(stderr, "[%s] UNLINK: %s (error: %s)\n",
                get_timestamp(), path, strerror(-error_code));
        return error_code;
    }

    error_code = check_access_permissions_impl(full_path, W_OK);
    if (error_code != 0) {
        free(full_path);
        fprintf(stderr, "[%s] UNLINK: %s (error: %s)\n",
                get_timestamp(), path, strerror(-error_code));
        return error_code;
    }

    int res = unlink(full_path);
    if (res == -1) {
        error_code = -errno;
        fprintf(stderr, "[%s] UNLINK: %s (error: %s)\n",
                get_timestamp(), path, strerror(-error_code));
    } else {
        fprintf(stderr, "[%s] UNLINK: %s (success)\n", get_timestamp(), path);
    }

    free(full_path);
    return error_code;
}

int myfuse_mkdir(const char *path, mode_t mode) {
    int error_code = 0;
    char *full_path = get_full_path_or_error(path, &error_code);
    if (!full_path) {
        fprintf(stderr, "[%s] MKDIR: %s (error: %s)\n",
                get_timestamp(), path, strerror(-error_code));
        return error_code;
    }

    int res = mkdir(full_path, mode);
    if (res == -1) {
        error_code = -errno;
        fprintf(stderr, "[%s] MKDIR: %s (error: %s)\n",
                get_timestamp(), path, strerror(-error_code));
    } else {
        fprintf(stderr, "[%s] MKDIR: %s (success)\n", get_timestamp(), path);
    }

    free(full_path);
    return error_code;
}

// Операция rmdir
int myfuse_rmdir(const char *path) {
    int error_code = 0;
    char *full_path = get_full_path_or_error(path, &error_code);
    if (!full_path) {
        fprintf(stderr, "[%s] RMDIR: %s (error: %s)\n",
                get_timestamp(), path, strerror(-error_code));
        return error_code;
    }

    int res = rmdir(full_path);
    if (res == -1) {
        error_code = -errno;
        fprintf(stderr, "[%s] RMDIR: %s (error: %s)\n",
                get_timestamp(), path, strerror(-error_code));
    } else {
        fprintf(stderr, "[%s] RMDIR: %s (success)\n", get_timestamp(), path);
    }

    free(full_path);
    return error_code;
}

void fill_operations(struct fuse_operations *ops) {
    ops->getattr = myfuse_getattr;
    ops->readdir = myfuse_readdir;
    ops->open = myfuse_open;
    ops->read = myfuse_read;
    ops->write = myfuse_write;
    ops->create = myfuse_create;
    ops->unlink = myfuse_unlink;
    ops->mkdir = myfuse_mkdir;
    ops->rmdir = myfuse_rmdir;
}