#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#define BUFFER_SIZE 4096
#define HZ 100  // Типичное значение HZ для Linux

// Структура для хранения информации о процессе
typedef struct {
    int pid;
    int ppid;
    int threads;
    char state;
    unsigned long utime;
    unsigned long stime;
    unsigned long vm_rss;
    unsigned long rss_anon;
    unsigned long rss_file;
    unsigned long voluntary_ctxt_switches;
    unsigned long nonvoluntary_ctxt_switches;
    unsigned long read_bytes;
    unsigned long write_bytes;
} process_info_t;

// Функция для форматирования размера в КиБ/МиБ
void format_size(unsigned long size_kb, char *output) {
    if (size_kb >= 1024) {
        sprintf(output, "%.2f MiB", (double)size_kb / 1024.0);
    } else {
        sprintf(output, "%lu KiB", size_kb);
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
    // Формат: pid comm state ppid ... utime stime ...
    char *token = strtok(buffer, " ");
    int field = 0;
    
    while (token != NULL && field < 50) {
        switch (field) {
            case 0: info->pid = atoi(token); break;
            case 2: info->state = token[0]; break;
            case 3: info->ppid = atoi(token); break;
            case 13: info->utime = atol(token); break;
            case 14: info->stime = atol(token); break;
            case 19: info->threads = atoi(token); break;
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
        // IO stats могут быть недоступны без прав root
        printf("Warning: /proc/%d/io not accessible (may require root)\n", pid);
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

// Вывод информации о процессе
void print_process_info(process_info_t *info) {
    char rss_formatted[64];
    char rss_anon_formatted[64];
    char rss_file_formatted[64];
    double cpu_time_sec;
    
    format_size(info->vm_rss, rss_formatted);
    format_size(info->rss_anon, rss_anon_formatted);
    format_size(info->rss_file, rss_file_formatted);
    
    cpu_time_sec = (double)(info->utime + info->stime) / HZ;
    
    printf("Process Statistics for PID %d:\n", info->pid);
    printf("================================\n");
    printf("PPid:                      %d\n", info->ppid);
    printf("Threads:                   %d\n", info->threads);
    printf("State:                     %c\n", info->state);
    printf("User time (ticks):         %lu\n", info->utime);
    printf("System time (ticks):       %lu\n", info->stime);
    printf("CPU time (seconds):        %.2f\n", cpu_time_sec);
    printf("Voluntary ctx switches:    %lu\n", info->voluntary_ctxt_switches);
    printf("Nonvoluntary ctx switches: %lu\n", info->nonvoluntary_ctxt_switches);
    printf("VmRSS:                     %s (%lu kB)\n", rss_formatted, info->vm_rss);
    printf("RssAnon:                   %s (%lu kB)\n", rss_anon_formatted, info->rss_anon);
    printf("RssFile:                   %s (%lu kB)\n", rss_file_formatted, info->rss_file);
    printf("Read bytes:                %lu\n", info->read_bytes);
    printf("Write bytes:               %lu\n", info->write_bytes);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <pid>\n", argv[0]);
        return 1;
    }
    
    int pid = atoi(argv[1]);
    if (pid <= 0) {
        fprintf(stderr, "Invalid PID: %s\n", argv[1]);
        return 1;
    }
    
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
    
    print_process_info(&info);
    
    return 0;
}