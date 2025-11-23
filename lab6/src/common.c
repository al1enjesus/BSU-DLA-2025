#include "operations.h"

// Общая функция для обработки аргументов командной строки FUSE программ
typedef struct {
    char *source_dir;
    char *mount_point;
    char **new_argv;
    int new_argc;
} fuse_args_parsed_t;

static void show_help_common(const char *progname, const char *description) {
    printf("Использование: %s [опции] <source_dir> <mount_point>\n", progname);
    printf("\n");
    printf("%s\n", description);
    printf("\n");
    printf("Опции файловой системы:\n");
    printf("    -f          Запуск в foreground режиме\n");
    printf("    -d          Включить debug вывод\n");
    printf("    -s          Однопоточный режим\n");
    printf("\n");
    printf("Пример:\n");
    printf("    %s /tmp/source /mnt/fuse\n", progname);
    printf("    %s -f /tmp/source /mnt/fuse\n", progname);
    printf("\n");
}

static int parse_fuse_args(int argc, char *argv[], fuse_args_parsed_t *parsed) {
    // Инициализация
    parsed->source_dir = NULL;
    parsed->mount_point = NULL;
    parsed->new_argv = NULL;
    parsed->new_argc = 0;
    
    // Проверяем минимальное количество аргументов
    if (argc < 3) {
        return -1;
    }
    
    // Найдем source_dir и mount_point (аргументы без -)
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            if (parsed->source_dir == NULL) {
                parsed->source_dir = argv[i];
            } else {
                parsed->mount_point = argv[i];
                break;
            }
        }
    }
    
    if (parsed->source_dir == NULL || parsed->mount_point == NULL) {
        return -1;
    }
    
    // Получаем абсолютный путь к базовой директории
    base_path = realpath(parsed->source_dir, NULL);
    if (base_path == NULL) {
        return -2; // Ошибка realpath
    }
    
    // Проверяем, что базовая директория существует и является директорией
    struct stat st;
    if (stat(base_path, &st) != 0) {
        free(base_path);
        base_path = NULL;
        return -3; // Директория не существует
    }
    
    if (!S_ISDIR(st.st_mode)) {
        free(base_path);
        base_path = NULL;
        return -4; // Не директория
    }
    
    // Создаем новый массив аргументов без source_dir
    parsed->new_argv = malloc(argc * sizeof(char*));
    if (!parsed->new_argv) {
        free(base_path);
        base_path = NULL;
        return -5; // Ошибка malloc
    }
    
    parsed->new_argv[parsed->new_argc++] = argv[0]; // имя программы
    
    for (int i = 1; i < argc; i++) {
        if (argv[i] != parsed->source_dir) {  // пропускаем source_dir
            parsed->new_argv[parsed->new_argc++] = argv[i];
        }
    }
    
    return 0; // Успех
}

static void cleanup_fuse_args(fuse_args_parsed_t *parsed) {
    if (parsed->new_argv) {
        free(parsed->new_argv);
        parsed->new_argv = NULL;
    }
    if (base_path) {
        free(base_path);
        base_path = NULL;
    }
}

int fuse_main_common(int argc, char *argv[], const char *description, 
                    struct fuse_operations *operations) {
    fuse_args_parsed_t parsed;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    int ret;
    
    // Проверяем помощь
    if (argc >= 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        show_help_common(argv[0], description);
        return 0;
    }
    
    // Парсим аргументы
    int parse_result = parse_fuse_args(argc, argv, &parsed);
    if (parse_result != 0) {
        const char *error_msg;
        switch (parse_result) {
            case -1: error_msg = "Необходимо указать source_dir и mount_point"; break;
            case -2: error_msg = "Не удалось получить абсолютный путь к source_dir"; break;
            case -3: error_msg = "source_dir не существует"; break;
            case -4: error_msg = "source_dir должен быть директорией"; break;
            case -5: error_msg = "Ошибка выделения памяти"; break;
            default: error_msg = "Неизвестная ошибка"; break;
        }
        fprintf(stderr, "Ошибка: %s\n\n", error_msg);
        show_help_common(argv[0], description);
        cleanup_fuse_args(&parsed);
        return 1;
    }
    
    printf("%s\n", description);
    printf("Source directory: %s\n", base_path);
    printf("Mount point: %s\n", parsed.mount_point);
    printf("Логирование операций в stderr\n");
    printf("Для размонтирования: fusermount -u %s\n", parsed.mount_point);
    printf("\n");
    
    // Обновляем args
    args.argc = parsed.new_argc;
    args.argv = parsed.new_argv;
    
    // Запускаем FUSE
    ret = fuse_main(args.argc, args.argv, operations, NULL);
    
    cleanup_fuse_args(&parsed);
    fuse_opt_free_args(&args);
    return ret;
}