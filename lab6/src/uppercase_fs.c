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

static void show_help(const char *progname) {
    printf("Использование: %s [опции] <source_dir> <mount_point>\n", progname);
    printf("\n");
    printf("Uppercase Filesystem:\n");
    printf("- Файлы на диске хранятся в оригинальном виде\n");
    printf("- При чтении все символы преобразуются в верхний регистр\n");
    printf("- Запись работает обычным образом\n");
    printf("\n");
    printf("Опции файловой системы:\n");
    printf("    -f          Запуск в foreground режиме\n");
    printf("    -d          Включить debug вывод\n");
    printf("    -s          Однопоточный режим\n");
    printf("\n");
    printf("Пример:\n");
    printf("    %s /tmp/source /mnt/fuse\n", progname);
    printf("    echo \"hello world\" > /tmp/source/test.txt\n");
    printf("    cat /tmp/source/test.txt     # Увидите: \"hello world\"\n");
    printf("    cat /mnt/fuse/test.txt       # Увидите: \"HELLO WORLD\"\n");
    printf("\n");
}

int main(int argc, char *argv[]) {
    int ret;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    
    // Обрабатываем аргументы командной строки
    if (argc < 3) {
        fprintf(stderr, "Ошибка: Необходимо указать source_dir и mount_point\n\n");
        show_help(argv[0]);
        return 1;
    }
    
    // Проверяем помощь
    if (argc >= 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        show_help(argv[0]);
        return 0;
    }
    
    // Найдем source_dir среди аргументов (последний аргумент, который не начинается с -)
    char *source_dir = NULL;
    char *mount_point = NULL;
    
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            if (source_dir == NULL) {
                source_dir = argv[i];
            } else {
                mount_point = argv[i];
                break;
            }
        }
    }
    
    if (source_dir == NULL || mount_point == NULL) {
        fprintf(stderr, "Ошибка: Необходимо указать source_dir и mount_point\n\n");
        show_help(argv[0]);
        return 1;
    }
    
    // Получаем базовую директорию
    base_path = realpath(source_dir, NULL);
    if (base_path == NULL) {
        perror("Ошибка: Не удалось получить абсолютный путь к source_dir");
        return 1;
    }
    
    // Проверяем, что базовая директория существует
    struct stat st;
    if (stat(base_path, &st) != 0) {
        perror("Ошибка: source_dir не существует");
        free(base_path);
        return 1;
    }
    
    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Ошибка: source_dir должен быть директорией\n");
        free(base_path);
        return 1;
    }
    
    printf("Uppercase FUSE filesystem\n");
    printf("Source directory: %s\n", base_path);
    printf("Mount point: %s\n", mount_point);
    printf("Преобразование: все символы в верхний регистр при чтении\n");
    printf("Логирование операций в stderr\n");
    printf("Для размонтирования: fusermount -u %s\n", mount_point);
    printf("\n");
    
    // Создаем новый список аргументов без source_dir
    char **new_argv = malloc(argc * sizeof(char*));
    int new_argc = 0;
    
    new_argv[new_argc++] = argv[0]; // имя программы
    
    for (int i = 1; i < argc; i++) {
        if (argv[i] != source_dir) {  // пропускаем source_dir
            new_argv[new_argc++] = argv[i];
        }
    }
    
    args.argc = new_argc;
    args.argv = new_argv;
    
    // Запускаем FUSE
    ret = fuse_main(args.argc, args.argv, &uppercase_operations, NULL);
    
    free(base_path);
    free(new_argv);
    fuse_opt_free_args(&args);
    return ret;
}