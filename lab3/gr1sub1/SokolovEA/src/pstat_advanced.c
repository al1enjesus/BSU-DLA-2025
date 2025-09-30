#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <getopt.h>

#define BUFFER_SIZE 4096
#define HZ 100  // Типичное значение HZ для Linux

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

// Функция для форматирования размера в КиБ/МиБ/ГиБ
void format_size(unsigned long size_kb, char *output) {
    if (size_kb >= 1024 * 1024) {
        sprintf(output, "%.2f GiB", (double)size_kb / (1024.0 * 1024.0));
    } else if (size_kb >= 1024) {
        sprintf(output, "%.2f MiB", (double)size_kb / 1024.0);
    } else {
        sprintf(output, "%lu KiB", size_kb);
    }
}

// Функция для форматирования байт
void format_bytes(unsigned long bytes, char *output) {
    if (bytes >= 1024 * 1024 * 1024) {
        sprintf(output, "%.2f GB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
    } else if (bytes >= 1024 * 1024) {
        sprintf(output, "%.2f MB", (double)bytes / (1024.0 * 1024.0));
    } else if (bytes >= 1024) {
        sprintf(output, "%.2f KB", (double)bytes / 1024.0);
    } else {
        sprintf(output, "%lu B", bytes);
    }
}

// Чтение /proc/<pid>/stat
int read_stat(int pid, process_info_t *info) {
    char path[256];
    FILE *file;
    char buffer[BUFFER_SIZE];
    
    sprintf(path, "/proc/%d/stat", pid);
    file = fopen(path, "r");
    if (!file) {
        perror("Failed to open stat file");
        return -1;
    }
    
    if (fgets(buffer, sizeof(buffer), file) == NULL) {
        fclose(file);
        return -1;
    }
    
    // Парсим основные поля из /proc/pid/stat
    char *token = strtok(buffer, " ");
    int field = 0;
    
    while (token != NULL && field < 50) {
        switch (field) {
            case 0: info->pid = atoi(token); break;
            case 1: 
                strncpy(info->comm, token, sizeof(info->comm) - 1);
                info->comm[sizeof(info->comm) - 1] = '\0';
                break;
            case 2: info->state = token[0]; break;
            case 3: info->ppid = atoi(token); break;
            case 13: info->utime = atol(token); break;
            case 14: info->stime = atol(token); break;
            case 19: info->threads = atoi(token); break;
            case 22: info->virt_mem = atol(token); break;
        }
        token = strtok(NULL, " ");
        field++;
    }
    
    fclose(file);
    return 0;
}

// Чтение /proc/<pid>/status
int read_status(int pid, process_info_t *info) {
    char path[256];
    FILE *file;
    char line[256];
    
    sprintf(path, "/proc/%d/status", pid);
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
    
    sprintf(path, "/proc/%d/io", pid);
    file = fopen(path, "r");
    if (!file) {
        if (getuid() != 0) {
            printf("Note: /proc/%d/io not accessible (requires root privileges)\n", pid);
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
void print_json(process_info_t *info) {
    double cpu_time_sec = (double)(info->utime + info->stime) / HZ;
    
    printf("{\n");
    printf("  \"pid\": %d,\n", info->pid);
    printf("  \"ppid\": %d,\n", info->ppid);
    printf("  \"command\": \"%s\",\n", info->comm);
    printf("  \"state\": \"%c\",\n", info->state);
    printf("  \"threads\": %d,\n", info->threads);
    printf("  \"utime_ticks\": %lu,\n", info->utime);
    printf("  \"stime_ticks\": %lu,\n", info->stime);
    printf("  \"cpu_time_seconds\": %.2f,\n", cpu_time_sec);
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
    
    format_size(info->vm_rss, rss_formatted);
    format_size(info->rss_anon, rss_anon_formatted);
    format_size(info->rss_file, rss_file_formatted);
    format_bytes(info->read_bytes, read_formatted);
    format_bytes(info->write_bytes, write_formatted);
    format_bytes(info->virt_mem, virt_formatted);
    
    cpu_time_sec = (double)(info->utime + info->stime) / HZ;
    
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
    
    printf("CPU time (seconds):        %.2f\n", cpu_time_sec);
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
        
        read_io(pid, &info); // Не критично, если не получится
        
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