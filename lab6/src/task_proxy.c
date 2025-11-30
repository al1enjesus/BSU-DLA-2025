#define _GNU_SOURCE
#define FUSE_USE_VERSION 26

#include <fuse.h> 
#include "task_proxy.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h> 

// Изменение: Переименование переменной
static const char *base_source_dir;

// Изменение: Новая функция логирования
static void log_operation(const char *operation_name, const char *path_in_fs, int result_code) {
    fprintf(stderr, "[%ld] FsProxy::%s: %s (Res: %d)\n", time(NULL), operation_name, path_in_fs, result_code);
}

/* ---------------- task_proxy_get_stat (getattr) ---------------- */
// FUSE 2 signature: no fi
static int task_proxy_get_stat(const char *path, struct stat *stat_buffer) {
    char full_physical_path[MAX_PATH_LEN];
    build_full_path(full_physical_path, base_source_dir, path);

    int func_res = lstat(full_physical_path, stat_buffer);
    int final_res = (func_res == -1) ? -errno : 0;
    
    log_operation("GETATTR", path, final_res);
    return final_res;
}

/* ---------------- task_proxy_list_dir (readdir) ---------------- */
// FUSE 2 signature: 4 аргумента
static int task_proxy_list_dir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset) {
    (void)offset; // Не используется
    
    char full_physical_path[MAX_PATH_LEN];
    build_full_path(full_physical_path, base_source_dir, path);

    DIR *dp = opendir(full_physical_path);
    if (!dp) {
        int final_res = -errno;
        log_operation("READDIR", path, final_res);
        return final_res;
    }

    struct dirent *de;
    while ((de = readdir(dp))) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12; 
        
        // FUSE 2 filler call (4 аргумента)
        if (filler(buffer, de->d_name, &st, 0)) {
            break; 
        }
    }

    closedir(dp);
    log_operation("READDIR", path, 0);
    return 0;
}

/* ---------------- task_proxy_open_file (open) ---------------- */
static int task_proxy_open_file(const char *path, struct fuse_file_info *fi) {
    char full_physical_path[MAX_PATH_LEN];
    build_full_path(full_physical_path, base_source_dir, path);

    int file_descriptor = open(full_physical_path, fi->flags);
    
    if (file_descriptor == -1) {
        int final_res = -errno;
        log_operation("OPEN", path, final_res);
        return final_res;
    }

    fi->fh = file_descriptor;
    log_operation("OPEN", path, 0);
    return 0;
}

/* ---------------- task_proxy_read_file (read) ---------------- */
static int task_proxy_read_file(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void)path;

    int bytes_read = pread(fi->fh, buf, size, offset);
    
    if (bytes_read == -1) {
        int final_res = -errno;
        log_operation("READ", path, final_res);
        return final_res;
    }

    log_operation("READ", path, bytes_read);
    return bytes_read;
}

/* ---------------- task_proxy_write_file (write) ---------------- */
static int task_proxy_write_file(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void)path;

    int bytes_written = pwrite(fi->fh, buf, size, offset);
    
    if (bytes_written == -1) {
        int final_res = -errno;
        log_operation("WRITE", path, final_res);
        return final_res;
    }
    
    log_operation("WRITE", path, bytes_written);
    return bytes_written;
}

/* ---------------- task_proxy_release_file (release) ---------------- */
static int task_proxy_release_file(const char *path, struct fuse_file_info *fi) {
    (void)path;
    
    int close_res = close(fi->fh);
    
    if (close_res == -1) {
        int final_res = -errno;
        log_operation("RELEASE", path, final_res);
        return final_res;
    }
    
    log_operation("RELEASE", path, 0);
    return 0;
}

/* ---------------- task_proxy_create_file (create) ---------------- */
static int task_proxy_create_file(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char full_physical_path[MAX_PATH_LEN];
    build_full_path(full_physical_path, base_source_dir, path);

    int file_descriptor = open(full_physical_path, fi->flags, mode);

    if (file_descriptor == -1) {
        int final_res = -errno;
        log_operation("CREATE", path, final_res);
        return final_res;
    }
    
    fi->fh = file_descriptor;
    log_operation("CREATE", path, 0);
    return 0;
}

/* ---------------- task_proxy_delete_file (unlink) ---------------- */
static int task_proxy_delete_file(const char *path) {
    char full_physical_path[MAX_PATH_LEN];
    build_full_path(full_physical_path, base_source_dir, path);

    int unlink_res = unlink(full_physical_path);
    int final_res = unlink_res == -1 ? -errno : 0;
    
    log_operation("UNLINK", path, final_res);
    return final_res;
}

/* ---------------- task_proxy_make_dir (mkdir) ---------------- */
static int task_proxy_make_dir(const char *path, mode_t mode) {
    char full_physical_path[MAX_PATH_LEN];
    build_full_path(full_physical_path, base_source_dir, path);

    int mkdir_res = mkdir(full_physical_path, mode);
    int final_res = mkdir_res == -1 ? -errno : 0;
    
    log_operation("MKDIR", path, final_res);
    return final_res;
}

/* ---------------- task_proxy_remove_dir (rmdir) ---------------- */
static int task_proxy_remove_dir(const char *path) {
    char full_physical_path[MAX_PATH_LEN];
    build_full_path(full_physical_path, base_source_dir, path);

    int rmdir_res = rmdir(full_physical_path);
    int final_res = rmdir_res == -1 ? -errno : 0;
    
    log_operation("RMDIR", path, final_res);
    return final_res;
}

// Структура операций для Passthrough
static struct fuse_operations proxy_fs_operations = {
    .getattr    = task_proxy_get_stat,
    .readdir    = task_proxy_list_dir,
    .open       = task_proxy_open_file,
    .read       = task_proxy_read_file,
    .write      = task_proxy_write_file,
    .release    = task_proxy_release_file,
    .create     = task_proxy_create_file,
    .unlink     = task_proxy_delete_file,
    .mkdir      = task_proxy_make_dir,
    .rmdir      = task_proxy_remove_dir,
};

const struct fuse_operations *task_proxy_get_operands(const char *source_path) {
    base_source_dir = source_path;
    return &proxy_fs_operations;
}