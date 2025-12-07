#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <ctime>
#include <cstring>
#include <fuse3/fuse.h>

// Общие утилиты для всех файловых систем
namespace fuse_utils {

// Получить текущее время в формате [YYYY-MM-DD HH:MM:SS]
inline std::string get_timestamp() {
    time_t now = time(nullptr);
    if (now == -1) {
        return "[timestamp-error]";
    }
    
    struct tm *t = localtime(&now);
    if (!t) {
        return "[timestamp-error]";
    }
    
    char buf[128];
    size_t written = strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
    if (written == 0) {
        return "[timestamp-error]";
    }
    return std::string(buf);
}

// Логирование операций
inline void log_operation(const char *op, const char *path, int result) {
    fprintf(stderr, "[%s] %s: %s (result: %d)\n", 
            get_timestamp().c_str(), op, path, result);
}

// Преобразование пути из FUSE в реальную ФС
inline std::string translate_path(const std::string& root, const char *path) {
    if (!path) {
        return root;
    }
    if (strcmp(path, "/") == 0) {
        return root;
    }
    return root + path;
}

// Проверка безопасности пути (защита от path traversal)
inline bool is_path_safe(const std::string& path) {
    if (path.empty()) {
        return true;
    }
    
    // Проверяем наличие попыток обхода директорий
    if (path.find("..") != std::string::npos) {
        fprintf(stderr, "Warning: Path traversal attempt detected: %s\n", path.c_str());
        return false;
    }
    
    // Проверяем на null bytes (потенциальные уязвимости)
    if (path.find('\0') != std::string::npos) {
        fprintf(stderr, "Warning: Null byte in path: %s\n", path.c_str());
        return false;
    }
    
    // Ограничение длины пути (стандарт PATH_MAX в Linux - 4096)
    const size_t MAX_PATH_LENGTH = 4096;
    if (path.length() > MAX_PATH_LENGTH) {
        fprintf(stderr, "Warning: Path too long: %lu characters\n", path.length());
        return false;
    }
    
    return true;
}

// Проверка указателя на null (для обычных указателей)
template<typename T>
inline bool check_null(const T* ptr, const char* context) {
    if (!ptr) {
        fprintf(stderr, "Error: Null pointer in %s\n", context);
        return false;
    }
    return true;
}

// Специализация для проверки filler (указатель на функцию)
inline bool check_filler(fuse_fill_dir_t filler, const char* context) {
    if (!filler) {
        fprintf(stderr, "Error: Null filler function in %s\n", context);
        return false;
    }
    return true;
}

} // namespace fuse_utils

#endif // UTILS_H