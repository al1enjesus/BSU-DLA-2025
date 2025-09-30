#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <errno.h>

#define BUFFER_SIZE 4096

// Структура для хранения информации о процессе
typedef struct {
    int pid;
    int ppid;
    int threads;
    char state;
    char comm[256];
    unsigned long utime;
    unsigned long stime;
    unsigned long vm_rss;
    unsigned long rss_anon;
    unsigned long rss_file;
    unsigned long voluntary_ctxt_switches;
    unsigned long nonvoluntary_ctxt_switches;
    unsigned long read_bytes;
    unsigned long write_bytes;
    unsigned long virt_mem;
} process_info_t;

// Опции программы
typedef struct {
    int verbose;
    int json_output;
    int monitor_mode;
    int interval;
} options_t;

// Глобальная переменная для HZ
static long hz_value = 0;

// Функция для получения значения HZ
long get_hz(void) {
    if (hz_value == 0) {
        hz_value = sysconf(_SC_CLK_TCK);
        if (hz_value <= 0) {
            hz_value = 100; // fallback значение
        }
    }
    return hz_value;
}

// Проверка существования процесса
int process_exists(int pid) {
    char path[256];
    struct stat statbuf;
    
    if (snprintf(path, sizeof(path), "/proc/%d", pid) >= (int)sizeof(path)) {
        return 0; // Путь слишком длинный
    }
    
    return (stat(path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode));
}

// Функция для форматирования размера в КиБ/МиБ/ГиБ
void format_size(unsigned long size_kb, char *output, size_t output_size) {
    if (size_kb >= 1024 * 1024) {
        snprintf(output, output_size, "%.2f GiB", (double)size_kb / (1024.0 * 1024.0));
    } else if (size_kb >= 1024) {
        snprintf(output, output_size, "%.2f MiB", (double)size_kb / 1024.0);
    } else {
        snprintf(output, output_size, "%lu KiB", size_kb);
    }
}

// Функция для форматирования байт
void format_bytes(unsigned long bytes, char *output, size_t output_size) {
    if (bytes >= 1024 * 1024 * 1024) {
        snprintf(output, output_size, "%.2f GB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
    } else if (bytes >= 1024 * 1024) {
        snprintf(output, output_size, "%.2f MB", (double)bytes / (1024.0 * 1024.0));
    } else if (bytes >= 1024) {
        snprintf(output, output_size, "%.2f KB", (double)bytes / 1024.0);
    } else {
        snprintf(output, output_size, "%lu B", bytes);
    }
}

// Чтение /proc/<pid>/stat
// Чтение /proc/<pid>/stat с улучшенным парсингом
int read_stat(int pid, process_info_t *info) {
    char path[256];
    FILE *file;
    char buffer[BUFFER_SIZE];
    
    if (snprintf(path, sizeof(path), "/proc/%d/stat", pid) >= (int)sizeof(path)) {
        fprintf(stderr, "Path too long for PID %d\n", pid);
        return -1;
    }
    
    file = fopen(path, "r");
    if (!file) {
        perror("Failed to open stat file");
        return -1;
    }
    
    if (fgets(buffer, sizeof(buffer), file) == NULL) {
        fclose(file);
        perror("Failed to read stat file");
        return -1;
    }
    fclose(file);
    
    // Улучшенный парсинг с учетом команд с пробелами
    // Находим первую и последнюю скобку для извлечения команды
    char *comm_start = strchr(buffer, '(');
    char *comm_end = strrchr(buffer, ')');
    
    if (!comm_start || !comm_end || comm_end <= comm_start) {
        fprintf(stderr, "Invalid stat format for PID %d\n", pid);
        return -1;
    }
    
    // Парсим PID (до первой скобки)
    info->pid = atoi(buffer);
    
    // Извлекаем команду
    size_t comm_len = comm_end - comm_start - 1;
    if (comm_len >= sizeof(info->comm)) {
        comm_len = sizeof(info->comm) - 1;
    }
    strncpy(info->comm, comm_start + 1, comm_len);
    info->comm[comm_len] = '\0';
    
    // Парсим остальные поля после последней скобки
    char *fields = comm_end + 1;
    if (*fields == ' ') fields++; // Пропускаем пробел после )
    
    // Используем sscanf для парсинга полей после команды
    int parsed = sscanf(fields, 
        "%c %d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu %*d %*d %*d %*d %d %*d %*d %lu",
        &info->state,     // поле 3
        &info->ppid,      // поле 4
        &info->utime,     // поле 14
        &info->stime,     // поле 15
        &info->threads,   // поле 20
        &info->virt_mem   // поле 23
    );
    
    if (parsed < 5) {
        fprintf(stderr, "Failed to parse required fields from stat for PID %d\n", pid);
        return -1;
    }
    
    return 0;
}

// Чтение /proc/<pid>/status
int read_status(int pid, process_info_t *info) {
    char path[256];
    FILE *file;
    char line[256];
    
    if (snprintf(path, sizeof(path), "/proc/%d/status", pid) >= (int)sizeof(path)) {
        fprintf(stderr, "Path too long for PID %d\n", pid);
        return -1;
    }
    
    file = fopen(path, "r");
    if (!file) {
        perror("Failed to open status file");
        return -1;
    }
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line, "VmRSS: %lu kB", &info->vm_rss);
        } else if (strncmp(line, "RssAnon:", 8) == 0) {
            sscanf(line, "RssAnon: %lu kB", &info->rss_anon);
        } else if (strncmp(line, "RssFile:", 8) == 0) {
            sscanf(line, "RssFile: %lu kB", &info->rss_file);
        } else if (strncmp(line, "voluntary_ctxt_switches:", 24) == 0) {
            sscanf(line, "voluntary_ctxt_switches: %lu", &info->voluntary_ctxt_switches);
        } else if (strncmp(line, "nonvoluntary_ctxt_switches:", 27) == 0) {
            sscanf(line, "nonvoluntary_ctxt_switches: %lu", &info->nonvoluntary_ctxt_switches);
        }
    }
    
    fclose(file);
    return 0;
}

// Чтение /proc/<pid>/io
int read_io(int pid, process_info_t *info) {
    char path[256];
    FILE *file;
    char line[256];
    
    if (snprintf(path, sizeof(path), "/proc/%d/io", pid) >= (int)sizeof(path)) {
        fprintf(stderr, "Path too long for PID %d\n", pid);
        info->read_bytes = 0;
        info->write_bytes = 0;
        return 0;
    }
    
    file = fopen(path, "r");
    if (!file) {
        // IO stats могут быть недоступны без прав root
        if (errno == EACCES) {
            printf("Note: /proc/%d/io not accessible (requires root privileges)\n", pid);
        } else {
            printf("Warning: /proc/%d/io not available\n", pid);
        }
        info->read_bytes = 0;
        info->write_bytes = 0;
        return 0;
    }
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "read_bytes:", 11) == 0) {
            sscanf(line, "read_bytes: %lu", &info->read_bytes);
        } else if (strncmp(line, "write_bytes:", 12) == 0) {
            sscanf(line, "write_bytes: %lu", &info->write_bytes);
        }
    }
    
    fclose(file);
    return 0;
}

// Вывод в JSON формате
// Вывод в JSON формате
void print_json(process_info_t *info) {
    double cpu_time_sec = (double)(info->utime + info->stime) / get_hz();
    
    printf("{\n");
    printf("  \"pid\": %d,\n", info->pid);
    printf("  \"ppid\": %d,\n", info->ppid);
    printf("  \"command\": \"%s\",\n", info->comm);
    printf("  \"state\": \"%c\",\n", info->state);
    printf("  \"threads\": %d,\n", info->threads);
    printf("  \"utime_ticks\": %lu,\n", info->utime);
    printf("  \"stime_ticks\": %lu,\n", info->stime);
    printf("  \"cpu_time_seconds\": %.2f,\n", cpu_time_sec);
    printf("  \"hz_value\": %ld,\n", get_hz());
    printf("  \"voluntary_ctxt_switches\": %lu,\n", info->voluntary_ctxt_switches);
    printf("  \"nonvoluntary_ctxt_switches\": %lu,\n", info->nonvoluntary_ctxt_switches);
    printf("  \"vm_rss_kb\": %lu,\n", info->vm_rss);
    printf("  \"rss_anon_kb\": %lu,\n", info->rss_anon);
    printf("  \"rss_file_kb\": %lu,\n", info->rss_file);
    printf("  \"virt_mem_bytes\": %lu,\n", info->virt_mem);
    printf("  \"read_bytes\": %lu,\n", info->read_bytes);
    printf("  \"write_bytes\": %lu\n", info->write_bytes);
    printf("}\n");
}

// Обычный вывод информации о процессе
void print_process_info(process_info_t *info, options_t *opts) {
    char rss_formatted[64];
    char rss_anon_formatted[64];
    char rss_file_formatted[64];
    char read_formatted[64];
    char write_formatted[64];
    char virt_formatted[64];
    double cpu_time_sec;
    long hz = get_hz();
    
    format_size(info->vm_rss, rss_formatted, sizeof(rss_formatted));
    format_size(info->rss_anon, rss_anon_formatted, sizeof(rss_anon_formatted));
    format_size(info->rss_file, rss_file_formatted, sizeof(rss_file_formatted));
    format_bytes(info->read_bytes, read_formatted, sizeof(read_formatted));
    format_bytes(info->write_bytes, write_formatted, sizeof(write_formatted));
    format_bytes(info->virt_mem, virt_formatted, sizeof(virt_formatted));
    
    cpu_time_sec = (double)(info->utime + info->stime) / hz;
    
    if (opts->json_output) {
        print_json(info);
        return;
    }
    
    printf("Process Statistics for PID %d:\n", info->pid);
    printf("================================\n");
    printf("Command:                   %s\n", info->comm);
    printf("PPid:                      %d\n", info->ppid);
    printf("Threads:                   %d\n", info->threads);
    printf("State:                     %c\n", info->state);
    
    if (opts->verbose) {
        printf("User time (ticks):         %lu\n", info->utime);
        printf("System time (ticks):       %lu\n", info->stime);
    }
    
    printf("CPU time (seconds):        %.2f (HZ=%ld)\n", cpu_time_sec, hz);
    printf("Voluntary ctx switches:    %lu\n", info->voluntary_ctxt_switches);
    printf("Nonvoluntary ctx switches: %lu\n", info->nonvoluntary_ctxt_switches);
    printf("VmRSS:                     %s (%lu kB)\n", rss_formatted, info->vm_rss);
    printf("RssAnon:                   %s (%lu kB)\n", rss_anon_formatted, info->rss_anon);
    printf("RssFile:                   %s (%lu kB)\n", rss_file_formatted, info->rss_file);
    
    if (opts->verbose) {
        printf("Virtual memory:            %s (%lu bytes)\n", virt_formatted, info->virt_mem);
    }
    
    printf("Read bytes:                %s (%lu)\n", read_formatted, info->read_bytes);
    printf("Write bytes:               %s (%lu)\n", write_formatted, info->write_bytes);
}

void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] <pid>\n", program_name);
    printf("Process statistics utility that reads /proc filesystem\n\n");
    printf("Options:\n");
    printf("  -v, --verbose      Show detailed information\n");
    printf("  -j, --json         Output in JSON format\n");
    printf("  -m, --monitor      Monitor mode (continuous updates)\n");
    printf("  -i, --interval=N   Update interval for monitor mode (default: 2 seconds)\n");
    printf("  -h, --help         Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s 1234           # Show stats for PID 1234\n", program_name);
    printf("  %s -v 1234        # Verbose output\n", program_name);
    printf("  %s -j 1234        # JSON output\n", program_name);
    printf("  %s -m -i 1 1234   # Monitor mode with 1 second interval\n", program_name);
}

int main(int argc, char *argv[]) {
    options_t opts = {0, 0, 0, 2}; // defaults
    int c;
    int pid;
    
    static struct option long_options[] = {
        {"verbose", no_argument, 0, 'v'},
        {"json", no_argument, 0, 'j'},
        {"monitor", no_argument, 0, 'm'},
        {"interval", required_argument, 0, 'i'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    while ((c = getopt_long(argc, argv, "vjmi:h", long_options, NULL)) != -1) {
        switch (c) {
            case 'v':
                opts.verbose = 1;
                break;
            case 'j':
                opts.json_output = 1;
                break;
            case 'm':
                opts.monitor_mode = 1;
                break;
            case 'i':
                opts.interval = atoi(optarg);
                if (opts.interval < 1) {
                    fprintf(stderr, "Interval must be at least 1 second\n");
                    return 1;
                }
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            case '?':
                print_usage(argv[0]);
                return 1;
        }
    }
    
    if (optind >= argc) {
        fprintf(stderr, "Error: PID argument required\n\n");
        print_usage(argv[0]);
        return 1;
    }
    
    pid = atoi(argv[optind]);
    if (pid <= 0) {
        fprintf(stderr, "Invalid PID: %s\n", argv[optind]);
        return 1;
    }
    
    // Проверяем существование процесса
    if (!process_exists(pid)) {
        fprintf(stderr, "Process with PID %d does not exist or is not accessible\n", pid);
        return 1;
    }
    
    do {
        process_info_t info;
        memset(&info, 0, sizeof(info));
        
        // Читаем информацию из различных файлов /proc
        if (read_stat(pid, &info) != 0) {
            fprintf(stderr, "Failed to read process statistics for PID %d\n", pid);
            return 1;
        }
        
        if (read_status(pid, &info) != 0) {
            fprintf(stderr, "Failed to read process status for PID %d\n", pid);
            return 1;
        }
        
        read_io(pid, &info); 
        
        if (opts.monitor_mode && !opts.json_output) {
            printf("\033[2J\033[H"); // Очистка экрана
            printf("=== Monitoring PID %d (press Ctrl+C to stop) ===\n\n", pid);
        }
        
        print_process_info(&info, &opts);
        
        if (opts.monitor_mode) {
            sleep(opts.interval);
        }
        
    } while (opts.monitor_mode);
    
    return 0;
}