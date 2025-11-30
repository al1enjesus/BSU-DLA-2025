#define _GNU_SOURCE
#define FUSE_USE_VERSION 26

#include <fuse.h> 
#include "task_monitor.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pthread.h> 
#include <stdarg.h> 

#define VIRTUAL_STAT_FILE "/.stats"
#define STAT_MAX_SIZE 1024

// Переименованные и объединенные счетчики
static long long op_counter_getattr = 0;
static long long op_counter_readdir = 0;
static long long op_counter_open = 0;
static long long op_counter_read = 0;
static long long op_counter_write = 0;
static long long op_counter_create = 0;
static long long op_counter_unlink = 0;
static long long op_counter_mkdir = 0;
static long long op_counter_rmdir = 0;
static long long data_flow_read = 0;
static long long data_flow_write = 0;

static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;
static const char *base_monitored_dir;


/**
 * @brief Генерирует содержимое виртуального файла.
 */
static size_t generate_stat_report(char *buffer, size_t max_size) {
    pthread_mutex_lock(&stats_mutex);
    
    int len = snprintf(buffer, max_size, 
        "*** Filesystem Usage Statistics ***\n"
        "Operations:\n"
        "  -> Stat/GetAttr: %lld\n"
        "  -> DirList/ReadDir: %lld\n"
        "  -> FileOpen: %lld\n"
        "  -> FileRead: %lld\n"
        "  -> FileWrite: %lld\n"
        "  -> FileCreate: %lld\n"
        "  -> FileDelete: %lld\n"
        "  -> DirCreate: %lld\n"
        "  -> DirDelete: %lld\n"
        "Data Throughput:\n"
        "  -> BytesRead: %lld\n"
        "  -> BytesWritten: %lld\n",
        op_counter_getattr,
        op_counter_readdir,
        op_counter_open,
        op_counter_read,
        op_counter_write,
        op_counter_create,
        op_counter_unlink,
        op_counter_mkdir,
        op_counter_rmdir,
        data_flow_read,
        data_flow_write
    );

    pthread_mutex_unlock(&stats_mutex);
    return len < max_size ? (size_t)len : max_size - 1;
}

/* ---------------- task_monitor_get_stat (getattr) ---------------- */
// FUSE 2 signature: no fi
static int task_monitor_get_stat(const char *path, struct stat *stat_buffer) {
    // 1. Виртуальный файл
    if (strcmp(path, VIRTUAL_STAT_FILE) == 0) {
        memset(stat_buffer, 0, sizeof(*stat_buffer));
        stat_buffer->st_mode = S_IFREG | 0444; 
        stat_buffer->st_nlink = 1;
        stat_buffer->st_size = STAT_MAX_SIZE; 
        return 0;
    }

    // 2. Passthrough и счетчик
    pthread_mutex_lock(&stats_mutex);
    op_counter_getattr++;
    pthread_mutex_unlock(&stats_mutex);

    char full_physical_path[MAX_PATH_LEN];
    build_full_path(full_physical_path, base_monitored_dir, path);

    int res = lstat(full_physical_path, stat_buffer);
    return res == -1 ? -errno : 0;
}

/* ---------------- task_monitor_list_dir (readdir) ---------------- */
// FUSE 2 signature: 4 аргумента
static int task_monitor_list_dir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset) {
    (void)offset; 
    
    pthread_mutex_lock(&stats_mutex);
    op_counter_readdir++;
    pthread_mutex_unlock(&stats_mutex);
    
    char full_physical_path[MAX_PATH_LEN];
    build_full_path(full_physical_path, base_monitored_dir, path);

    DIR *dp = opendir(full_physical_path);
    if (!dp) {
        return -errno;
    }
    
    // Passthrough для содержимого
    struct dirent *de;
    while ((de = readdir(dp))) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;

        if (filler(buffer, de->d_name, &st, 0)) { // FUSE 2 filler
            closedir(dp);
            return 0;
        }
    }
    closedir(dp);

    // Добавление виртуального файла только в корне
    if (strcmp(path, "/") == 0) {
        struct stat st_stats;
        memset(&st_stats, 0, sizeof(st_stats));
        st_stats.st_mode = S_IFREG | 0444;
        
        filler(buffer, VIRTUAL_STAT_FILE + 1, &st_stats, 0); 
    }
    
    return 0;
}

/* ---------------- task_monitor_open_file (open) ---------------- */
static int task_monitor_open_file(const char *path, struct fuse_file_info *fi) {
    if (strcmp(path, VIRTUAL_STAT_FILE) == 0) {
        fi->fh = 0;
    } else {
        char full_physical_path[MAX_PATH_LEN];
        build_full_path(full_physical_path, base_monitored_dir, path);

        int fd = open(full_physical_path, fi->flags);
        if (fd == -1) {
            return -errno;
        }
        fi->fh = fd;
    }
    
    pthread_mutex_lock(&stats_mutex);
    op_counter_open++;
    pthread_mutex_unlock(&stats_mutex);
    
    return 0;
}

/* ---------------- task_monitor_read_file (read) ---------------- */
static int task_monitor_read_file(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    ssize_t res;
    
    // 1. Виртуальный файл
    if (fi->fh == 0) {
        char stats_buffer[STAT_MAX_SIZE];
        size_t stats_len = generate_stat_report(stats_buffer, STAT_MAX_SIZE);
        
        res = 0;
        if (offset < stats_len) {
            size_t copy_size = stats_len - offset;
            if (copy_size > size) copy_size = size;
            
            memcpy(buf, stats_buffer + offset, copy_size);
            res = (ssize_t)copy_size;
        }
        return (int)res;
    }

    // 2. Passthrough и счетчики
    res = pread(fi->fh, buf, size, offset);

    if (res > 0) {
        pthread_mutex_lock(&stats_mutex);
        op_counter_read++;
        data_flow_read += res;
        pthread_mutex_unlock(&stats_mutex);
    }

    return res == -1 ? -errno : (int)res;
}

/* ---------------- task_monitor_write_file (write) ---------------- */
static int task_monitor_write_file(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    // 1. Запрет записи в виртуальный файл
    if (fi->fh == 0) {
        return -EACCES; 
    }
    
    // 2. Passthrough и счетчики
    ssize_t res = pwrite(fi->fh, buf, size, offset);

    if (res > 0) {
        pthread_mutex_lock(&stats_mutex);
        op_counter_write++;
        data_flow_write += res;
        pthread_mutex_unlock(&stats_mutex);
    }

    return res == -1 ? -errno : (int)res;
}

/* ---------------- task_monitor_release_file (release) ---------------- */
static int task_monitor_release_file(const char *path, struct fuse_file_info *fi) {
    (void)path; 
    
    int res = 0;
    if (fi->fh != 0) {
        res = close(fi->fh);
    }

    return res == -1 ? -errno : 0;
}


/* ---------------- task_monitor_create_file (create) ---------------- */
static int task_monitor_create_file(const char *path, mode_t mode, struct fuse_file_info *fi) {
    if (strcmp(path, VIRTUAL_STAT_FILE) == 0) return -EACCES;
    
    char full_physical_path[MAX_PATH_LEN];
    build_full_path(full_physical_path, base_monitored_dir, path);
    
    pthread_mutex_lock(&stats_mutex);
    op_counter_create++;
    pthread_mutex_unlock(&stats_mutex);

    // open() с O_CREAT для создания файла
    int file_descriptor = open(full_physical_path, fi->flags, mode);

    if (file_descriptor == -1) {
        return -errno;
    }
    
    // Передаем дескриптор FUSE
    fi->fh = file_descriptor;
    return 0;
}

/* ---------------- task_monitor_delete_file (unlink) ---------------- */
static int task_monitor_delete_file(const char *path) {
    if (strcmp(path, VIRTUAL_STAT_FILE) == 0) return -EACCES;
    
    char full_physical_path[MAX_PATH_LEN];
    build_full_path(full_physical_path, base_monitored_dir, path);
    
    pthread_mutex_lock(&stats_mutex);
    op_counter_unlink++;
    pthread_mutex_unlock(&stats_mutex);

    int unlink_res = unlink(full_physical_path);
    return unlink_res == -1 ? -errno : 0;
}

/* ---------------- task_monitor_make_dir (mkdir) ---------------- */
static int task_monitor_make_dir(const char *path, mode_t mode) {
    char full_physical_path[MAX_PATH_LEN];
    build_full_path(full_physical_path, base_monitored_dir, path);

    pthread_mutex_lock(&stats_mutex);
    op_counter_mkdir++;
    pthread_mutex_unlock(&stats_mutex);

    int mkdir_res = mkdir(full_physical_path, mode);
    return mkdir_res == -1 ? -errno : 0;
}

/* ---------------- task_monitor_remove_dir (rmdir) ---------------- */
static int task_monitor_remove_dir(const char *path) {
    char full_physical_path[MAX_PATH_LEN];
    build_full_path(full_physical_path, base_monitored_dir, path);

    pthread_mutex_lock(&stats_mutex);
    op_counter_rmdir++;
    pthread_mutex_unlock(&stats_mutex);

    int rmdir_res = rmdir(full_physical_path);
    return rmdir_res == -1 ? -errno : 0;
}


// Структура операций для Monitoring FS
static struct fuse_operations monitor_fs_operations = {
    .getattr    = task_monitor_get_stat,
    .readdir    = task_monitor_list_dir,
    .open       = task_monitor_open_file,
    .read       = task_monitor_read_file,
    .write      = task_monitor_write_file,
    .release    = task_monitor_release_file,
    .create     = task_monitor_create_file,
    .unlink     = task_monitor_delete_file,
    .mkdir      = task_monitor_make_dir,
    .rmdir      = task_monitor_remove_dir,
};

const struct fuse_operations *task_monitor_get_operands(const char *source_path) {
    base_monitored_dir = source_path;
    return &monitor_fs_operations;
}