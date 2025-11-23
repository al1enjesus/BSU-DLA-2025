/*
 * Задание C: Uppercase Filesystem
 * 
 * FUSE файловая система, которая преобразует всё содержимое файлов 
 * в верхний регистр при чтении.
 * 
 * Использование: ./uppercase_fs <source_dir> <mount_point>
 * 
 * Компиляция: make uppercase_fs
 */

#include "operations.h"

static struct fuse_operations uppercase_operations = {
    .getattr    = passthrough_getattr,  // Метаданные не изменяются
    .readdir    = passthrough_readdir,  // Имена файлов не изменяются
    .open       = passthrough_open,
    .read       = uppercase_read,       // Преобразование в uppercase при чтении
    .write      = passthrough_write,    // Запись обычная
    .create     = passthrough_create,
    .unlink     = passthrough_unlink,
    .mkdir      = passthrough_mkdir,
    .rmdir      = passthrough_rmdir,
};

int main(int argc, char *argv[]) {
    return fuse_main_common(argc, argv, 
        "Uppercase FUSE filesystem\nПреобразование: все символы в верхний регистр при чтении", 
        &uppercase_operations);
}