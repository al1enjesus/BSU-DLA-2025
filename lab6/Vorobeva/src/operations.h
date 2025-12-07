#ifndef OPERATIONS_H
#define OPERATIONS_H

#include <string>
#include <ctime>
#include <cstring>
#include <fuse3/fuse.h>

// Утилиты, общие для всех файловых систем
namespace fuse_utils {

// Получить текущее время в формате [YYYY-MM-DD HH:MM:SS]
static std::string get_timestamp() {
    time_t now = time(nullptr);
    struct tm *t = localtime(&now);
    char buf[128];
    size_t written = strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
    if (written == 0) {
        return "[timestamp-error]";
    }
    return std::string(buf);
}

// Логирование операций
static void log_operation(const char *op, const char *path, int result) {
    fprintf(stderr, "[%s] %s: %s (result: %d)\n", 
            get_timestamp().c_str(), op, path, result);
}

// Преобразование пути из FUSE в реальную ФС
static std::string translate_path(const std::string& root, const char *path) {
    if (strcmp(path, "/") == 0)
        return root;
    return root + path;
}

// Проверка безопасности пути (защита от path traversal)
static bool is_path_safe(const std::string& path) {
    // Проверяем наличие попыток обхода директорий
    if (path.find("..") != std::string::npos) {
        return false;
    }
    
    // Проверяем символьные ссылки (упрощённо)
    if (path.length() > 256) {  // Ограничение длины пути
        return false;
    }
    
    return true;
}

// Объявления общих функций FUSE операций
extern void set_root_path(const std::string& path);
extern int pt_getattr(const char *path, struct stat *stbuf,
                      struct fuse_file_info *fi);
extern int pt_mkdir(const char *path, mode_t mode);
extern int pt_unlink(const char *path);
extern int pt_rmdir(const char *path);
extern int pt_rename(const char *from, const char *to, unsigned int flags);
extern int pt_truncate(const char *path, off_t size,
                       struct fuse_file_info *fi);
extern int pt_open(const char *path, struct fuse_file_info *fi);
extern int pt_read(const char *path, char *buf, size_t size,
                   off_t offset, struct fuse_file_info *fi);
extern int pt_write(const char *path, const char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi);
extern int pt_release(const char *path, struct fuse_file_info *fi);
extern int pt_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags);
extern int pt_create(const char *path, mode_t mode, struct fuse_file_info *fi);

} // namespace fuse_utils

#endif // OPERATIONS_H