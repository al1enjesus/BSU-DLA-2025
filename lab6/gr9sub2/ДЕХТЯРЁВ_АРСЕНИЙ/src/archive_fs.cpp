#define FUSE_USE_VERSION 35
#include <fuse3/fuse.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <algorithm>
#include <limits>

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
static int tar_fd = -1;

static size_t oct2dec(const char *str, size_t size) {
    size_t result = 0;
    for (size_t i = 0; i < size && str[i] && str[i] != ' '; i++) {
        if (str[i] >= '0' && str[i] <= '7') {
            // Проверка на переполнение
            if (result > (std::numeric_limits<size_t>::max() >> 3)) {
                return 0;
            }
            result = (result << 3) + (str[i] - '0');
        } else {
            break;
        }
    }
    return result;
}

static char get_file_type(const char *flag) {
    if (flag[0] == '5') return 'd';
    if (flag[0] == '0') return 'f';
    if (flag[0] == '\0') return 'f';
    if (flag[0] == '2') return 'l'; // symlink
    return 'f';
}

static bool validate_tar_header(const char* header) {
    // Базовая проверка контрольной суммы
    unsigned int checksum = 0;
    unsigned int recorded_checksum = 0;
    
    for (int i = 0; i < 148; i++) checksum += (unsigned char)header[i];
    for (int i = 148; i < 156; i++) checksum += ' ';
    for (int i = 156; i < 512; i++) checksum += (unsigned char)header[i];
    
    recorded_checksum = oct2dec(header + 148, 8);
    
    return checksum == recorded_checksum;
}

static void parse_tar() {
    tar_fd = open(tar_path.c_str(), O_RDONLY);
    if (tar_fd < 0) {
        fprintf(stderr, "Error: Cannot open tar file '%s': %s\n", 
                tar_path.c_str(), strerror(errno));
        exit(1);
    }

    char header[512];
    while (true) {
        ssize_t r = read(tar_fd, header, 512);
        if (r == 0) break;
        if (r < 0) {
            fprintf(stderr, "Error reading tar file: %s\n", strerror(errno));
            break;
        }
        if (r != 512) break;

        if (header[0] == '\0')
            break;

        // Базовая валидация заголовка
        if (!validate_tar_header(header)) {
            fprintf(stderr, "Warning: Invalid tar header, skipping\n");
            continue;
        }

        TarEntry e;
        // Безопасное извлечение имени (ограничение длины)
        e.name = std::string(header, 100);
        size_t null_pos = e.name.find('\0');
        if (null_pos != std::string::npos) {
            e.name.resize(null_pos);
        }
        
        // Проверка на пустое имя
        if (e.name.empty()) {
            continue;
        }

        e.size = oct2dec(header + 124, 12);
        e.mode = oct2dec(header + 100, 8);
        e.data_offset = lseek(tar_fd, 0, SEEK_CUR);
        
        char type = get_file_type(header + 156);
        e.is_dir = (type == 'd');

        files[e.name] = e;

        size_t blocks = (e.size + 511) / 512;
        if (lseek(tar_fd, blocks * 512, SEEK_CUR) < 0) {
            fprintf(stderr, "Error seeking in tar file: %s\n", strerror(errno));
            break;
        }
    }

    // Построение структуры директорий
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
    st->st_atime = st->st_mtime = st->st_ctime = time(nullptr);

    if (strcmp(path, "/") == 0) {
        st->st_mode = S_IFDIR | 0555;
        st->st_nlink = 2;
        return 0;
    }

    std::string p = path + 1;
    
    if (directories.count(p)) {
        st->st_mode = S_IFDIR | 0555;
        st->st_nlink = 2;
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

    auto hd = std::make_unique<HandleData>();
    hd->offset = e.data_offset;
    hd->size = e.size;

    fi->fh = reinterpret_cast<uint64_t>(hd.release());
    fi->direct_io = 1;
    return 0;
}

static int fs_read(const char *path, char *buf, size_t size,
                   off_t offset, struct fuse_file_info *fi) {

    auto *hd = reinterpret_cast<HandleData*>(fi->fh);

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
    delete reinterpret_cast<HandleData*>(fi->fh);
    return 0;
}

static struct fuse_operations ops = {};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s archive.tar <mountpoint>\n", argv[0]);
        return 1;
    }

    tar_path = argv[1];
    
    // Проверка существования файла
    struct stat st;
    if (stat(tar_path.c_str(), &st) != 0) {
        fprintf(stderr, "Error: Cannot access tar file '%s': %s\n", 
                tar_path.c_str(), strerror(errno));
        return 1;
    }
    
    // Проверка что это обычный файл
    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "Error: '%s' is not a regular file\n", tar_path.c_str());
        return 1;
    }

    parse_tar();

    ops.getattr = fs_getattr;
    ops.readdir = fs_readdir;
    ops.open = fs_open;
    ops.read = fs_read;
    ops.release = fs_release;

    int result = fuse_main(argc - 1, argv + 1, &ops, nullptr);
    
    if (tar_fd >= 0) {
        close(tar_fd);
    }
    
    return result;
}
