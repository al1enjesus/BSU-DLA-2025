#define FUSE_USE_VERSION 35
#include <fuse3/fuse.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

struct TarEntry {
    std::string name;
    mode_t mode;
    size_t size;
    off_t data_offset;
};

static std::map<std::string, TarEntry> files;
static std::string tar_path;
static int tar_fd;


static size_t oct2dec(const char *str, size_t size) {
    size_t result = 0;
    for (size_t i = 0; i < size && str[i]; i++) {
        if (str[i] >= '0' && str[i] <= '7')
            result = (result << 3) + (str[i] - '0');
    }
    return result;
}

static void parse_tar() {
    tar_fd = open(tar_path.c_str(), O_RDONLY);
    if (tar_fd < 0) {
        perror("tar open");
        exit(1);
    }

    while (1) {
        char header[512];
        ssize_t r = read(tar_fd, header, 512);
        if (r == 0) break;
        if (r < 0) break;

        if (header[0] == '\0')
            break; // конец архива

        TarEntry e;
        e.name = std::string(header, 100);
        e.name = e.name.c_str(); // trim nulls
        e.size = oct2dec(header + 124, 12);
        e.mode = oct2dec(header + 100, 8);
        e.data_offset = lseek(tar_fd, 0, SEEK_CUR);

        files[e.name] = e;

        // пропускаем данные
        size_t blocks = (e.size + 511) / 512;
        lseek(tar_fd, blocks * 512, SEEK_CUR);
    }
}


static int fs_getattr(const char *path, struct stat *st,
                      struct fuse_file_info *fi) {

    memset(st, 0, sizeof(*st));

    if (strcmp(path, "/") == 0) {
        st->st_mode = S_IFDIR | 0555;
        st->st_nlink = 2;
        return 0;
    }

    std::string p = path + 1; 
    if (files.count(p) == 0)
        return -ENOENT;

    const TarEntry &e = files[p];

    st->st_mode = S_IFREG | 0444;
    st->st_size = e.size;
    st->st_nlink = 1;

    return 0;
}

static int fs_readdir(const char *path, void *buf,
                       fuse_fill_dir_t filler, off_t offset,
                       struct fuse_file_info *fi,
                       enum fuse_readdir_flags flags) {

    filler(buf, ".", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, "..", nullptr, 0, FUSE_FILL_DIR_PLUS);

    for (auto &kv : files) {
        filler(buf, kv.first.c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
    }

    return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi) {
    std::string p = path + 1;
    if (!files.count(p)) return -ENOENT;

    fi->direct_io = 1;
    fi->nonseekable = 0;

    return 0;
}

static int fs_read(const char *path, char *buf, size_t size,
                   off_t offset, struct fuse_file_info *fi) {

    std::string p = path + 1;
    if (!files.count(p)) return -ENOENT;

    const TarEntry &e = files[p];

    if (offset >= e.size)
        return 0;

    if (offset + size > e.size)
        size = e.size - offset;

    lseek(tar_fd, e.data_offset + offset, SEEK_SET);
    ssize_t r = read(tar_fd, buf, size);
    if (r < 0) return -errno;

    return r;
}

static struct fuse_operations ops = {};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s archive.tar <mountpoint>\n", argv[0]);
        return 1;
    }

    tar_path = argv[1];

    parse_tar();

    ops.getattr = fs_getattr;
    ops.readdir = fs_readdir;
    ops.open = fs_open;
    ops.read = fs_read;

    return fuse_main(argc - 1, argv + 1, &ops, nullptr);
}

