/*
 * Задание B: ROT13 Encryption Filesystem
 * 
 * FUSE файловая система с простым ROT13 шифрованием.
 * Файлы на диске хранятся зашифрованными, при чтении автоматически расшифровываются.
 * 
 * Использование: ./rot13_fs <source_dir> <mount_point>
 * 
 * Компиляция: make rot13_fs
 */

#include "operations.h"

static struct fuse_operations rot13_operations = {
    .getattr    = passthrough_getattr,  // Метаданные не шифруются
    .readdir    = passthrough_readdir,  // Имена файлов не шифруются
    .open       = passthrough_open,
    .read       = rot13_read,           // Расшифровка при чтении
    .write      = rot13_write,          // Шифрование при записи
    .create     = passthrough_create,
    .unlink     = passthrough_unlink,
    .mkdir      = passthrough_mkdir,
    .rmdir      = passthrough_rmdir,
};

int main(int argc, char *argv[]) {
    return fuse_main_common(argc, argv, 
        "ROT13 Encryption FUSE filesystem\nШифрование: ROT13 (сдвиг на 13 позиций в алфавите)", 
        &rot13_operations);
}