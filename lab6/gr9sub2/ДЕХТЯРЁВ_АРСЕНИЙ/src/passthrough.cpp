#define FUSE_USE_VERSION 35
#include <fuse3/fuse.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

static std::string root;

static std::string translate(const char *path) {
    if (strcmp(path, "/") == 0)
        return root;
    return root + path;
}

// getattr
static int pt_getattr(const char *path, struct stat *stbuf,
                      struct fuse_file_info *fi) {
    std::string p = translate(path);
    int res;

    if (fi != nullptr && fi->fh > 0)
        res = fstat(fi->fh, stbuf);
    else
        res = lstat(p.c_str(), stbuf);

    return res == -1 ? -errno : 0;
}

// open
static int pt_open(const char *path, struct fuse_file_info *fi) {
    std::string p = translate(path);
    int fd = open(p.c_str(), fi->flags);
    if (fd == -1)
        return -errno;

    fi->fh = fd;
    return 0;
}

// read
static int pt_read(const char *path, char *buf, size_t size,
                   off_t offset, struct fuse_file_info *fi) {
    int fd = fi->fh;
    int res = pread(fd, buf, size, offset);
    return res == -1 ? -errno : res;
}

// write
static int pt_write(const char *path, const char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi) {
    int fd = fi->fh;
    int res = pwrite(fd, buf, size, offset);
    return res == -1 ? -errno : res;
}

// release
static int pt_release(const char *path, struct fuse_file_info *fi) {
    close(fi->fh);
    return 0;
}

// mkdir
static int pt_mkdir(const char *path, mode_t mode) {
    std::string p = translate(path);
    int res = mkdir(p.c_str(), mode);
    return res == -1 ? -errno : 0;
}

// rmdir
static int pt_rmdir(const char *path) {
    std::string p = translate(path);
    int res = rmdir(p.c_str());
    return res == -1 ? -errno : 0;
}

// unlink
static int pt_unlink(const char *path) {
    std::string p = translate(path);
    int res = unlink(p.c_str());
    return res == -1 ? -errno : 0;
}

// rename
static int pt_rename(const char *from, const char *to, unsigned int flags) {
    std::string f = translate(from);
    std::string t = translate(to);
    int res = rename(f.c_str(), t.c_str());
    return res == -1 ? -errno : 0;
}

// readdir
static int pt_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags) {
    std::string p = translate(path);

    DIR *dp = opendir(p.c_str());
    if (dp == nullptr)
        return -errno;

    struct dirent *de;
    while ((de = readdir(dp)) != nullptr) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;

        if (filler(buf, de->d_name, &st, 0, FUSE_FILL_DIR_PLUS))
            break;
    }

    closedir(dp);
    return 0;
}

// create
static int pt_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    std::string p = translate(path);
    int fd = open(p.c_str(), fi->flags, mode);
    if (fd == -1)
        return -errno;

    fi->fh = fd;
    return 0;
}

// truncate
static int pt_truncate(const char *path, off_t size,
                       struct fuse_file_info *fi) {
    int res;
    if (fi != nullptr)
        res = ftruncate(fi->fh, size);
    else {
        std::string p = translate(path);
        res = truncate(p.c_str(), size);
    }
    return res == -1 ? -errno : 0;
}

// symlink
static int pt_symlink(const char *to, const char *from) {
    std::string f = translate(from);
    int res = symlink(to, f.c_str());
    return res == -1 ? -errno : 0;
}

// readlink
static int pt_readlink(const char *path, char *buf, size_t size) {
    std::string p = translate(path);
    int res = readlink(p.c_str(), buf, size - 1); // Оставляем место для нуль-терминатора
    if (res >= 0) {
        buf[res] = '\0'; // Гарантируем нуль-терминацию
    }
    return res == -1 ? -errno : res;
}

// chmod
static int pt_chmod(const char *path, mode_t mode, struct fuse_file_info *fi) {
    std::string p = translate(path);
    int res = chmod(p.c_str(), mode);
    return res == -1 ? -errno : 0;
}

// chown
static int pt_chown(const char *path, uid_t uid, gid_t gid,
                    struct fuse_file_info *fi) {
    std::string p = translate(path);
    int res = lchown(p.c_str(), uid, gid);
    return res == -1 ? -errno : 0;
}

// utimens
static int pt_utimens(const char *path, const struct timespec tv[2],
                      struct fuse_file_info *fi) {
    std::string p = translate(path);
    int res = utimensat(AT_FDCWD, p.c_str(), tv, AT_SYMLINK_NOFOLLOW);
    return res == -1 ? -errno : 0;
}

// fsync
static int pt_fsync(const char *path, int datasync,
                    struct fuse_file_info *fi) {
    int fd = fi->fh;
    int res = datasync ? fdatasync(fd) : fsync(fd);
    return res == -1 ? -errno : 0;
}

static struct fuse_operations pt_ops = {};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mountpoint>\n", argv[0]);
        return 1;
    }

    char *real_root = realpath(argv[1], nullptr);
    if (!real_root) {
        fprintf(stderr, "Error: Cannot resolve real path for '%s': %s\n", 
                argv[1], strerror(errno));
        return 1;
    }
    root = real_root;
    free(real_root); // Исправление утечки памяти

    pt_ops.getattr = pt_getattr;
    pt_ops.open = pt_open;
    pt_ops.read = pt_read;
    pt_ops.write = pt_write;
    pt_ops.release = pt_release;
    pt_ops.readdir = pt_readdir;
    pt_ops.mkdir = pt_mkdir;
    pt_ops.rmdir = pt_rmdir;
    pt_ops.unlink = pt_unlink;
    pt_ops.rename = pt_rename;
    pt_ops.create = pt_create;
    pt_ops.truncate = pt_truncate;
    pt_ops.symlink = pt_symlink;
    pt_ops.readlink = pt_readlink;
    pt_ops.chmod = pt_chmod;
    pt_ops.chown = pt_chown;
    pt_ops.utimens = pt_utimens;
    pt_ops.fsync = pt_fsync;

    return fuse_main(argc - 1, argv + 1, &pt_ops, nullptr);
}
