#define FUSE_USE_VERSION 35
#include <fuse3/fuse.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <map>

static std::string root;


struct Stats {
    uint64_t reads = 0;
    uint64_t writes = 0;
    uint64_t opens = 0;
    uint64_t bytes_read = 0;
    uint64_t bytes_written = 0;
};

static Stats stats;


static std::string real_path(const char *path) {
    if (strcmp(path, "/.stats") == 0)
        return ""; // виртуальный файл

    if (strcmp(path, "/") == 0)
        return root;

    return root + path;
}

static bool is_stats(const char *path) {
    return strcmp(path, "/.stats") == 0;
}


static int fs_getattr(const char *path, struct stat *st,
                      struct fuse_file_info *fi) {
    memset(st, 0, sizeof(*st));

    if (is_stats(path)) {
        st->st_mode = S_IFREG | 0444;
        st->st_size = 1024;
        return 0;
    }

    std::string p = real_path(path);
    struct stat s;
    if (lstat(p.c_str(), &s) == -1) return -errno;

    *st = s;
    return 0;
}

static int fs_readdir(const char *path, void *buf,
                       fuse_fill_dir_t filler, off_t offset,
                       struct fuse_file_info *fi,
                       enum fuse_readdir_flags flags) {

    filler(buf, ".", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, "..", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, ".stats", nullptr, 0, FUSE_FILL_DIR_PLUS);

    DIR *dp = opendir(root.c_str());
    if (!dp) return -errno;

    struct dirent *de;
    while ((de = readdir(dp)) != nullptr) {
        filler(buf, de->d_name, nullptr, 0, FUSE_FILL_DIR_PLUS);
    }
    closedir(dp);
    return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi) {
    stats.opens++;

    if (is_stats(path))
        return 0;

    std::string p = real_path(path);

    int fd = open(p.c_str(), fi->flags);
    if (fd < 0) return -errno;

    fi->fh = fd;
    return 0;
}

static int fs_read(const char *path, char *buf, size_t size,
                   off_t offset, struct fuse_file_info *fi) {

    if (is_stats(path)) {
        stats.reads++;

        std::string out =
            "reads: " + std::to_string(stats.reads) + "\n" +
            "writes: " + std::to_string(stats.writes) + "\n" +
            "opens: " + std::to_string(stats.opens) + "\n" +
            "bytes_read: " + std::to_string(stats.bytes_read) + "\n" +
            "bytes_written: " + std::to_string(stats.bytes_written) + "\n";

        if (offset >= out.size())
            return 0;

        if (offset + size > out.size())
            size = out.size() - offset;

        memcpy(buf, out.c_str() + offset, size);
        return size;
    }

    stats.reads++;

    int fd = fi->fh;
    int r = pread(fd, buf, size, offset);
    if (r < 0) return -errno;

    stats.bytes_read += r;
    return r;
}

static int fs_write(const char *path, const char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi) {

    if (is_stats(path)) return -EACCES;

    stats.writes++;

    int fd = fi->fh;
    int r = pwrite(fd, buf, size, offset);
    if (r < 0) return -errno;

    stats.bytes_written += r;
    return r;
}

static int fs_release(const char *path, struct fuse_file_info *fi) {
    if (!is_stats(path))
        close(fi->fh);
    return 0;
}

static struct fuse_operations ops = {};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <source_dir> <mountpoint>\n", argv[0]);
        return 1;
    }

    root = realpath(argv[1], nullptr);

    ops.getattr = fs_getattr;
    ops.readdir = fs_readdir;
    ops.open = fs_open;
    ops.read = fs_read;
    ops.write = fs_write;
    ops.release = fs_release;

    return fuse_main(argc - 1, argv + 1, &ops, nullptr);
}

