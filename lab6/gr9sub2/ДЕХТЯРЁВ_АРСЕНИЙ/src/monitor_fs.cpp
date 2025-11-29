#define FUSE_USE_VERSION 35
#include <fuse3/fuse.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <atomic>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

static std::string root;

struct Stats {
    std::atomic<uint64_t> reads{0};
    std::atomic<uint64_t> writes{0};
    std::atomic<uint64_t> opens{0};
    std::atomic<uint64_t> bytes_read{0};
    std::atomic<uint64_t> bytes_written{0};
};

static Stats stats;

static bool is_stats(const char *path) {
    return strcmp(path, "/.stats") == 0;
}

static std::string fs_translate(const char *path) {
    if (strcmp(path, "/") == 0)
        return root;
    return root + path;
}

static int fs_getattr(const char *path, struct stat *st,
                      struct fuse_file_info *fi)
{
    memset(st, 0, sizeof(*st));

    if (is_stats(path)) {
        st->st_mode = S_IFREG | 0444;
        st->st_nlink = 1;
        st->st_size = 256; // Достаточно для статистики
        return 0;
    }

    std::string p = fs_translate(path);

    if (fi && fi->fh > 0)
        return fstat(fi->fh, st) == -1 ? -errno : 0;
    else
        return lstat(p.c_str(), st) == -1 ? -errno : 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi)
{
    stats.opens++;

    if (is_stats(path))
        return 0;

    std::string p = fs_translate(path);

    int fd = open(p.c_str(), fi->flags);
    if (fd < 0)
        return -errno;

    fi->fh = fd;
    return 0;
}

static int fs_read(const char *path, char *buf, size_t size,
                   off_t offset, struct fuse_file_info *fi)
{
    stats.reads++;

    if (is_stats(path)) {
        // Атомарное чтение статистики
        uint64_t reads = stats.reads.load();
        uint64_t writes = stats.writes.load();
        uint64_t opens = stats.opens.load();
        uint64_t bytes_read = stats.bytes_read.load();
        uint64_t bytes_written = stats.bytes_written.load();
        
        std::string out =
            "reads: " + std::to_string(reads) + "\n" +
            "writes: " + std::to_string(writes) + "\n" +
            "opens: " + std::to_string(opens) + "\n" +
            "bytes_read: " + std::to_string(bytes_read) + "\n" +
            "bytes_written: " + std::to_string(bytes_written) + "\n";

        if (offset >= (off_t)out.size())
            return 0;

        if (offset + size > out.size())
            size = out.size() - offset;

        memcpy(buf, out.data() + offset, size);
        return size;
    }

    int fd = fi->fh;
    int r = pread(fd, buf, size, offset);
    if (r < 0)
        return -errno;

    stats.bytes_read += r;
    return r;
}

static int fs_write(const char *path, const char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi)
{
    if (is_stats(path))
        return -EACCES;

    stats.writes++;

    int fd = fi->fh;
    int r = pwrite(fd, buf, size, offset);

    if (r < 0)
        return -errno;

    stats.bytes_written += r;
    return r;
}

static int fs_release(const char *path, struct fuse_file_info *fi)
{
    if (!is_stats(path))
        close(fi->fh);
    return 0;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags)
{
    filler(buf, ".", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, "..", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, ".stats", nullptr, 0, FUSE_FILL_DIR_PLUS);

    std::string p = fs_translate(path);
    DIR *dp = opendir(p.c_str());
    if (!dp)
        return -errno;

    struct dirent *de;
    while ((de = readdir(dp))) {
        filler(buf, de->d_name, nullptr, 0, FUSE_FILL_DIR_PLUS);
    }

    closedir(dp);
    return 0;
}

static int fs_mkdir(const char *path, mode_t mode)
{
    if (is_stats(path))
        return -EACCES;

    std::string p = fs_translate(path);
    return mkdir(p.c_str(), mode) == -1 ? -errno : 0;
}

static int fs_rmdir(const char *path)
{
    if (is_stats(path))
        return -EACCES;

    std::string p = fs_translate(path);
    return rmdir(p.c_str()) == -1 ? -errno : 0;
}

static int fs_unlink(const char *path)
{
    if (is_stats(path))
        return -EACCES;

    std::string p = fs_translate(path);
    return unlink(p.c_str()) == -1 ? -errno : 0;
}

static int fs_rename(const char *from, const char *to, unsigned int flags)
{
    if (is_stats(from) || is_stats(to))
        return -EACCES;

    std::string f = fs_translate(from);
    std::string t = fs_translate(to);
    return rename(f.c_str(), t.c_str()) == -1 ? -errno : 0;
}

static int fs_create(const char *path, mode_t mode,
                     struct fuse_file_info *fi)
{
    if (is_stats(path))
        return -EACCES;

    stats.opens++;

    std::string p = fs_translate(path);

    int fd = open(p.c_str(), fi->flags | O_CREAT | O_TRUNC, mode);
    if (fd < 0)
        return -errno;

    fi->fh = fd;
    return 0;
}

static int fs_truncate(const char *path, off_t size,
                       struct fuse_file_info *fi)
{
    if (is_stats(path))
        return -EACCES;

    if (fi)
        return ftruncate(fi->fh, size) == -1 ? -errno : 0;

    std::string p = fs_translate(path);
    return truncate(p.c_str(), size) == -1 ? -errno : 0;
}

static int fs_symlink(const char *to, const char *from)
{
    if (is_stats(from))
        return -EACCES;

    std::string f = fs_translate(from);
    return symlink(to, f.c_str()) == -1 ? -errno : 0;
}

static int fs_readlink(const char *path, char *buf, size_t size)
{
    if (is_stats(path))
        return -EACCES;

    std::string p = fs_translate(path);
    int r = readlink(p.c_str(), buf, size - 1);
    if (r >= 0) {
        buf[r] = '\0';
    }
    return r == -1 ? -errno : r;
}

static int fs_chmod(const char *path, mode_t mode,
                    struct fuse_file_info *fi)
{
    if (is_stats(path))
        return -EACCES;

    std::string p = fs_translate(path);
    return chmod(p.c_str(), mode) == -1 ? -errno : 0;
}

static int fs_chown(const char *path, uid_t uid, gid_t gid,
                    struct fuse_file_info *fi)
{
    if (is_stats(path))
        return -EACCES;

    std::string p = fs_translate(path);
    return lchown(p.c_str(), uid, gid) == -1 ? -errno : 0;
}

static int fs_utimens(const char *path, const struct timespec tv[2],
                      struct fuse_file_info *fi)
{
    if (is_stats(path))
        return -EACCES;

    std::string p = fs_translate(path);
    return utimensat(AT_FDCWD, p.c_str(), tv, AT_SYMLINK_NOFOLLOW)
        == -1 ? -errno : 0;
}

static int fs_fsync(const char *path, int datasync,
                    struct fuse_file_info *fi)
{
    if (is_stats(path))
        return -EACCES;

    int fd = fi->fh;
    int r = datasync ? fdatasync(fd) : fsync(fd);
    return r == -1 ? -errno : 0;
}

static struct fuse_operations ops = {};

int main(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Usage: %s <source_dir> <mountpoint>\n", argv[0]);
        return 1;
    }

    char *real_root = realpath(argv[1], nullptr);
    if (!real_root) {
        fprintf(stderr, "Error: Cannot resolve real path for '%s': %s\n", 
                argv[1], strerror(errno));
        return 1;
    }
    root = real_root;
    free(real_root);

    ops.getattr  = fs_getattr;
    ops.open     = fs_open;
    ops.read     = fs_read;
    ops.write    = fs_write;
    ops.release  = fs_release;
    ops.readdir  = fs_readdir;
    ops.mkdir    = fs_mkdir;
    ops.rmdir    = fs_rmdir;
    ops.unlink   = fs_unlink;
    ops.rename   = fs_rename;
    ops.create   = fs_create;
    ops.truncate = fs_truncate;
    ops.symlink  = fs_symlink;
    ops.readlink = fs_readlink;
    ops.chmod    = fs_chmod;
    ops.chown    = fs_chown;
    ops.utimens  = fs_utimens;
    ops.fsync    = fs_fsync;

    return fuse_main(argc - 1, argv + 1, &ops, nullptr);
}
