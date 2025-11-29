#define FUSE_USE_VERSION 35
#include <fuse3/fuse.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <algorithm>

struct TarEntry {
    std::string name;
    mode_t mode;
    size_t size;
    off_t data_offset;
    bool is_dir;
};

static std::map<std::string, TarEntry> files;
static std::map<std::string, std::set<std::string>> directories;
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

static char get_file_type(const char *flag) {
    if (flag[0] == '5') return 'd';
    if (flag[0] == '0') return 'f';
    if (flag[0] == '\0') return 'f';
    return 'f';
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
            break;

        TarEntry e;
        e.name = std::string(header, 100);
        e.name = e.name.c_str(); // trim
        e.size = oct2dec(header + 124, 12);
        e.mode = oct2dec(header + 100, 8);
        e.data_offset = lseek(tar_fd, 0, SEEK_CUR);
        
        char type = get_file_type(header + 156);
        e.is_dir = (type == 'd');

        files[e.name] = e;

        size_t blocks = (e.size + 511) / 512;
        lseek(tar_fd, blocks * 512, SEEK_CUR);
    }

    for (const auto& entry : files) {
        const std::string& full_path = entry.first;
        const TarEntry& e = entry.second;
        
        if (e.is_dir) {
            std::string dir_name = full_path;
            if (!dir_name.empty() && dir_name.back() == '/') {
                dir_name.pop_back();
            }
            
            size_t pos = dir_name.find_last_of('/');
            if (pos != std::string::npos) {
                std::string parent_dir = dir_name.substr(0, pos);
                std::string base_name = dir_name.substr(pos + 1);
                directories[parent_dir].insert(base_name);
            } else {
                directories[""].insert(dir_name);
            }
            
            directories[dir_name];
        } else {
            size_t pos = full_path.find_last_of('/');
            if (pos != std::string::npos) {
                std::string parent_dir = full_path.substr(0, pos);
                std::string file_name = full_path.substr(pos + 1);
                directories[parent_dir].insert(file_name);
                
                directories[parent_dir];
            } else {
                directories[""].insert(full_path);
            }
        }
    }

    directories[""];
}

static int fs_getattr(const char *path, struct stat *st,
                      struct fuse_file_info *fi) {

    memset(st, 0, sizeof(*st));
    st->st_uid = getuid();
    st->st_gid = getgid();

    if (strcmp(path, "/") == 0) {
        st->st_mode = S_IFDIR | 0555;
        st->st_nlink = 2;
        return 0;
    }

    std::string p = path + 1;
    
    if (directories.count(p)) {
        if (files.count(p) && files[p].is_dir) {
            st->st_mode = S_IFDIR | 0555;
            st->st_nlink = 2;
        } else {
            st->st_mode = S_IFDIR | 0555;
            st->st_nlink = 2;
        }
        return 0;
    }

    if (files.count(p)) {
        const TarEntry &e = files[p];
        if (e.is_dir) {
            st->st_mode = S_IFDIR | 0555;
            st->st_nlink = 2;
        } else {
            st->st_mode = S_IFREG | 0444;
            st->st_size = e.size;
            st->st_nlink = 1;
        }
        return 0;
    }

    return -ENOENT;
}

static int fs_readdir(const char *path, void *buf,
                      fuse_fill_dir_t filler, off_t offset,
                      struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags) {

    filler(buf, ".", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, "..", nullptr, 0, FUSE_FILL_DIR_PLUS);

    std::string dir_path = (strcmp(path, "/") == 0) ? "" : (path + 1);
    
    if (directories.count(dir_path)) {
        for (const auto& entry : directories[dir_path]) {
            filler(buf, entry.c_str(), nullptr, 0, FUSE_FILL_DIR_PLUS);
        }
    }

    return 0;
}

struct HandleData {
    off_t offset;
    size_t size;
};

static int fs_open(const char *path, struct fuse_file_info *fi) {
    if (strcmp(path, "/") == 0)
        return -EISDIR;

    std::string p = path + 1;
    
    if (directories.count(p))
        return -EISDIR;

    if (!files.count(p))
        return -ENOENT;

    const TarEntry &e = files[p];
    
    if (e.is_dir)
        return -EISDIR;

    auto *hd = new HandleData;
    hd->offset = e.data_offset;
    hd->size = e.size;

    fi->fh = (uint64_t)hd;
    fi->direct_io = 1;
    return 0;
}

static int fs_read(const char *path, char *buf, size_t size,
                   off_t offset, struct fuse_file_info *fi) {

    auto *hd = (HandleData*)fi->fh;

    if (offset >= hd->size)
        return 0;

    if (offset + size > hd->size)
        size = hd->size - offset;

    off_t real_offset = hd->offset + offset;

    if (lseek(tar_fd, real_offset, SEEK_SET) < 0)
        return -errno;

    ssize_t r = read(tar_fd, buf, size);
    if (r < 0) return -errno;

    return r;
}

static int fs_release(const char *path, struct fuse_file_info *fi) {
    delete (HandleData*)fi->fh;
    return 0;
}

static struct fuse_operations ops = {};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s archive.tar <mountpoint>\n", argv[0]);
        return 1;
    }

    tar_path = argv[1];
    parse_tar();

    // Debug
    printf("Parsed files:\n");
    for (const auto& f : files) {
        printf("  %s %s\n", f.second.is_dir ? "DIR " : "FILE", f.first.c_str());
    }
    printf("Directory structure:\n");
    for (const auto& d : directories) {
        printf("  Dir '%s':", d.first.c_str());
        for (const auto& entry : d.second) {
            printf(" %s", entry.c_str());
        }
        printf("\n");
    }

    ops.getattr = fs_getattr;
    ops.readdir = fs_readdir;
    ops.open = fs_open;
    ops.read = fs_read;
    ops.release = fs_release;

    return fuse_main(argc - 1, argv + 1, &ops, nullptr);
}