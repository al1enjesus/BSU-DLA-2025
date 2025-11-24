#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/xattr.h> 
#include <sys/types.h> 
#include <limits.h>    
#include <ctype.h>

static char *base_path = NULL;

static void log_operation(const char *op, const char *path, int result) {
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    fprintf(stderr, "[%s] %s: %s (result: %d)\n", timestamp, op, path, result);
}

static void get_full_path(char *fullpath, const char *path) {
    char target_path[PATH_MAX];

    if (strcmp(path, "/") == 0) {
        strcpy(target_path, base_path);
    } else {
        snprintf(target_path, PATH_MAX, "%s%s", base_path, path);
    }

    if (realpath(target_path, fullpath) == NULL) {
        strncpy(fullpath, target_path, PATH_MAX);
        fullpath[PATH_MAX - 1] = '\0';
        return;
    }
    // @claude ПРОВЕРКА БЕЗОПАСНОСТИ: Убеждаемся, что канонический путь начинается с base_path.
    // Если путь выходит за пределы base_path (например, через ../), то len будет меньше base_path.
    size_t base_len = strlen(base_path);
    if (strncmp(fullpath, base_path, base_len) != 0) {
        fprintf(stderr, "SECURITY ALERT: Path traversal denied for: %s\n", path);
        strcpy(fullpath, base_path);
    }
}

static void rot13(char *buf, size_t size) {
    for (size_t i = 0; i < size; i++) {
        char c = buf[i];
        if (c >= 'a' && c <= 'z') {
            buf[i] = 'a' + (c - 'a' + 13) % 26;
        } else if (c >= 'A' && c <= 'Z') {
            buf[i] = 'A' + (c - 'A' + 13) % 26;
        }
    }
}


static int passthrough_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    char fullpath[PATH_MAX];
    get_full_path(fullpath, path);

    int res = lstat(fullpath, stbuf);

    if (res == -1) {
        log_operation("GETATTR", path, -errno);
        return -errno;
    }

    log_operation("GETATTR", path, 0);
    return 0;
}

static int passthrough_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                                off_t offset, struct fuse_file_info *fi,
                                enum fuse_readdir_flags flags) {
    (void) offset; (void) fi; (void) flags;
    char fullpath[PATH_MAX];
    get_full_path(fullpath, path);

    DIR *dp = opendir(fullpath);
    if (dp == NULL) {
        log_operation("READDIR", path, -errno);
        return -errno;
    }

    struct dirent *de;
    int res = 0;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;

        if (filler(buf, de->d_name, &st, 0, 0)) {
            res = -ENOMEM;
            break;
        }
    }

    closedir(dp);
    log_operation("READDIR", path, res);
    return res;
}

static int passthrough_open(const char *path, struct fuse_file_info *fi) {
    char fullpath[PATH_MAX];
    get_full_path(fullpath, path);

    int fd = open(fullpath, fi->flags);
    if (fd == -1) {
        log_operation("OPEN", path, -errno);
        return -errno;
    }

    close(fd);

    log_operation("OPEN", path, 0);
    return 0;
}

static int passthrough_read(const char *path, char *buf, size_t size, off_t offset,
                            struct fuse_file_info *fi) {
    (void) fi;
    char fullpath[PATH_MAX];
    get_full_path(fullpath, path);

    int fd = open(fullpath, O_RDONLY);
    if (fd == -1) {
        log_operation("READ", path, -errno);
        return -errno;
    }

    int res = pread(fd, buf, size, offset);
    
    if (res > 0) {
        rot13(buf, res); 
    }
    
    if (res == -1)
        res = -errno;

    close(fd);
    log_operation("READ", path, res);
    return res;
}

static int passthrough_write(const char *path, const char *buf, size_t size,
                             off_t offset, struct fuse_file_info *fi) {
    (void) fi;
    char fullpath[PATH_MAX];
    get_full_path(fullpath, path);

    int fd = open(fullpath, O_WRONLY); 
    if (fd == -1) {
        log_operation("WRITE", path, -errno);
        return -errno;
    }

    char *encrypted_buf = (char*)malloc(size);
    if (encrypted_buf == NULL) {
        close(fd);
        log_operation("WRITE", path, -ENOMEM);
        return -ENOMEM;
    }
    
    memcpy(encrypted_buf, buf, size);
    rot13(encrypted_buf, size);

    int res = pwrite(fd, encrypted_buf, size, offset);
    
    if (res == -1)
        res = -errno;

    free(encrypted_buf);
    close(fd);
    log_operation("WRITE", path, res);
    return res;
}

static int passthrough_create(const char *path, mode_t mode,
                               struct fuse_file_info *fi) {
    (void) fi;
    char fullpath[PATH_MAX];
    get_full_path(fullpath, path);

    int res = creat(fullpath, mode);
    if (res == -1) {
        log_operation("CREATE", path, -errno);
        return -errno;
    }

    close(res);

    log_operation("CREATE", path, 0);
    return 0;
}

static int passthrough_unlink(const char *path) {
    char fullpath[PATH_MAX];
    get_full_path(fullpath, path);

    int res = unlink(fullpath);

    if (res == -1) {
        log_operation("UNLINK", path, -errno);
        return -errno;
    }

    log_operation("UNLINK", path, 0);
    return 0;
}

static int passthrough_mkdir(const char *path, mode_t mode) {
    char fullpath[PATH_MAX];
    get_full_path(fullpath, path);

    int res = mkdir(fullpath, mode);

    if (res == -1) {
        log_operation("MKDIR", path, -errno);
        return -errno;
    }

    log_operation("MKDIR", path, 0);
    return 0;
}

static int passthrough_rmdir(const char *path) {
    char fullpath[PATH_MAX];
    get_full_path(fullpath, path);

    int res = rmdir(fullpath);

    if (res == -1) {
        log_operation("RMDIR", path, -errno);
        return -errno;
    }

    log_operation("RMDIR", path, 0);
    return 0;
}

static int passthrough_truncate(const char *path, off_t size, struct fuse_file_info *fi) {
    (void) fi;
    char fullpath[PATH_MAX];
    get_full_path(fullpath, path);

    int res = truncate(fullpath, size);

    if (res == -1) {
        log_operation("TRUNCATE", path, -errno);
        return -errno;
    }

    log_operation("TRUNCATE", path, 0);
    return 0;
}

static int passthrough_chmod(const char *path, mode_t mode,
                                struct fuse_file_info *fi) {
    (void) fi;
    char fullpath[PATH_MAX];
    get_full_path(fullpath, path);

    int res = chmod(fullpath, mode);

    if (res == -1) {
        log_operation("CHMOD", path, -errno);
        return -errno;
    }

    log_operation("CHMOD", path, 0);
    return 0;
}

static int passthrough_chown(const char *path, uid_t uid, gid_t gid,
                                struct fuse_file_info *fi) {
    (void) fi;
    char fullpath[PATH_MAX];
    get_full_path(fullpath, path);

    int res = lchown(fullpath, uid, gid);

    if (res == -1) {
        log_operation("CHOWN", path, -errno);
        return -errno;
    }

    log_operation("CHOWN", path, 0);
    return 0;
}

static struct fuse_operations passthrough_oper = {
    .getattr    = passthrough_getattr,
    .readdir    = passthrough_readdir,
    .open       = passthrough_open,
    .read       = passthrough_read,
    .write      = passthrough_write,
    .create     = passthrough_create,
    .unlink     = passthrough_unlink,
    .mkdir      = passthrough_mkdir,
    .rmdir      = passthrough_rmdir,
    .truncate   = passthrough_truncate,
    .chmod      = passthrough_chmod,
    .chown      = passthrough_chown,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mount_point> [fuse_options]\n", argv[0]);
        return 1;
    }

    base_path = realpath(argv[1], NULL);
    if (base_path == NULL) {
        perror("realpath");
        return 1;
    }

    fprintf(stderr, "Mounting %s at %s\n", base_path, argv[2]);

    int fuse_argc = argc - 1;
    char **fuse_argv = malloc(sizeof(char*) * fuse_argc);
    if (!fuse_argv) {
        perror("malloc");
        free(base_path); // @claude Освобождаем память при ошибке malloc
        return 1;
    }
    
    fuse_argv[0] = argv[0]; 
    for (int i = 2; i < argc; i++) {
        fuse_argv[i-1] = argv[i];
    }

    int ret = fuse_main(fuse_argc, fuse_argv, &passthrough_oper, NULL);

    free(fuse_argv);
    free(base_path);

    return ret;
}