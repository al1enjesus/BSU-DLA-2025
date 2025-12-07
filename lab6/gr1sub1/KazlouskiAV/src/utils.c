#include "../include/fuse_fs.h"

struct fuse_fs_config config = {0};
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

long stats_reads = 0;
long stats_writes = 0;
long stats_opens = 0;
long long stats_bytes_read = 0;
long long stats_bytes_written = 0;

void log_op(const char *op, const char *path, int result) {
    char timestamp[64];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(stderr, "[%s] %s: %s (result: %d)\n", timestamp, op, path, result);
}

int ends_with(const char *str, const char *suffix) {
    if (!str || !suffix) return 0;
    size_t len_str = strlen(str);
    size_t len_suf = strlen(suffix);
    return (len_str >= len_suf) && (strcmp(str + len_str - len_suf, suffix) == 0);
}

int is_stats_file(const char *path) {
    return strcmp(path, "/.stats") == 0;
}

int stats_getattr(const char *path, struct stat *stbuf) {
    char buf[512];
    int len = snprintf(buf, sizeof(buf),
                       "reads: %ld\nwrites: %ld\nopens: %ld\nbytes_read: %lld\nbytes_written: %lld\n",
                       stats_reads, stats_writes, stats_opens, stats_bytes_read, stats_bytes_written);
    memset(stbuf, 0, sizeof(*stbuf));
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = len;
    return 0;
}

int stats_readdir(const char *path, void *buf, fuse_fill_dir_t filler) {
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    filler(buf, ".stats", NULL, 0, 0);
    // остальные файлы — из source_dir
    return 0; // вызывается только из passthrough
}

int stats_read(const char *path, char *buf, size_t size, off_t offset) {
    char data[512];
    int len = snprintf(data, sizeof(data),
                       "reads: %ld\nwrites: %ld\nopens: %ld\nbytes_read: %lld\nbytes_written: %lld\n",
                       stats_reads, stats_writes, stats_opens, stats_bytes_read, stats_bytes_written);

    if (offset >= len) return 0;
    if (offset + (off_t)size > len) size = len - offset;
    memcpy(buf, data + offset, size);
    return (int)size;
}