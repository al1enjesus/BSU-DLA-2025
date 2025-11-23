/*
 * Задание A: Passthrough FUSE filesystem
 * 
 * Простая FUSE файловая система, которая "зеркалирует" существующую директорию
 * с логированием всех операций.
 * 
 * Использование: ./passthrough_fs <source_dir> <mount_point>
 * 
 * Компиляция: make passthrough_fs
 */

#include "operations.h"

static struct fuse_operations passthrough_operations = {
    .getattr    = passthrough_getattr,
    .readdir    = passthrough_readdir,
    .open       = passthrough_open,
    .read       = passthrough_read,
    .write      = passthrough_write,
    .create     = passthrough_create,
    .unlink     = passthrough_unlink,
    .mkdir      = passthrough_mkdir,
    .rmdir      = passthrough_rmdir,
};

int main(int argc, char *argv[]) {
    return fuse_main_common(argc, argv, 
        "Passthrough FUSE filesystem", 
        &passthrough_operations);
}