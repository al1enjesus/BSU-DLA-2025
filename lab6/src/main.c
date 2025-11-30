#define FUSE_USE_VERSION 26

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fuse.h> 

#include "utils.h"
#include "task_proxy.h"
#include "task_archive.h"
#include "task_monitor.h"

static struct fuse_operations *fs_ops_pointer;

static void display_startup_info(const char *prog_name)
{
    fprintf(stderr,
        "Usage:\\n"
        "  %s passthrough <source_directory> <mount_point> [fuse options]\\n"
        "  %s archive <tar_file_path> <mount_point> [fuse options]\\n"
        "  %s monitor <source_directory> <mount_point> [fuse options]\\n",
        prog_name, prog_name, prog_name);
}

int main(int argc, char *argv[])
{
    if (argc < 4) {
        display_startup_info(argv[0]);
        return 1;
    }

    const char *operation_mode = argv[1];
    const char *source_location = argv[2];
    const char *mount_target = argv[3];

    /* 1. Выбор и инициализация файловой системы */
    if (strcmp(operation_mode, "passthrough") == 0) {
        fs_ops_pointer = (struct fuse_operations *)task_proxy_get_operands(source_location);
    } else if (strcmp(operation_mode, "archive") == 0) {
        fs_ops_pointer = (struct fuse_operations *)task_archive_get_operands(source_location);
    } else if (strcmp(operation_mode, "monitor") == 0) {
        fs_ops_pointer = (struct fuse_operations *)task_monitor_get_operands(source_location);
    } else {
        fprintf(stderr, "ERROR: Undefined operating mode: %s\\n", operation_mode);
        display_startup_info(argv[0]);
        return 1;
    }
    
    if (fs_ops_pointer == NULL) {
         fprintf(stderr, "ERROR: Filesystem initialization failed.\\n");
         return 1;
    }

    /* 2. Формируем массив аргументов для fuse_main */
    int new_argc = 0;
    char **new_argv = (char**)calloc(argc, sizeof(char*));
    if (!new_argv) {
        perror("calloc failed");
        return 1;
    }

    new_argv[new_argc++] = argv[0]; 

    // Добавляем точку монтирования
    new_argv[new_argc++] = (char*)mount_target;

    // Копируем все аргументы FUSE, которые шли после точки монтирования
    for (int i = 4; i < argc; i++) {
        new_argv[new_argc++] = argv[i];
    }
    
    // Добавляем опцию, чтобы FUSE работал в многопоточном режиме по умолчанию 
    // и разрешил root-доступ (часто необходимо для FUSE 2)
    fuse_opt_add_arg((struct fuse_args *)&new_argv, "-oallow_other");
    
    fprintf(stderr, "Startup: Initiating FUSE daemon for mode '%s'...\n", operation_mode);

    // 3. Запуск FUSE. NULL в последнем аргументе.
    int fuse_result = fuse_main(new_argc, new_argv, fs_ops_pointer, NULL);

    free(new_argv);
    
    return fuse_result;
}