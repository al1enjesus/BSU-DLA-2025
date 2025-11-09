#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#define MAX_BUF 4096
#define HZ 100  // стандартное значение HZ для Linux

// Структура для хранения данных из /proc/<pid>/stat
typedef struct {
    int pid;
    char comm[256];
    char state;
    int ppid;
    unsigned long utime;
    unsigned long stime;
    long num_threads;
} proc_stat_t;

// Структура для данных из /proc/<pid>/status
typedef struct {
    unsigned long vm_rss;
    unsigned long rss_anon;
    unsigned long rss_file;
    unsigned long voluntary_ctxt_switches;
    unsigned long nonvoluntary_ctxt_switches;
} proc_status_t;

// Структура для данных из /proc/<pid>/io
typedef struct {
    unsigned long read_bytes;
    unsigned long write_bytes;
} proc_io_t;

// Функция для чтения файла в буфер
int read_proc_file(const char *path, char *buf, size_t buf_size) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        return -1;
    }
    
    ssize_t bytes_read = read(fd, buf, buf_size - 1);
    close(fd);
    
    if (bytes_read <= 0) {
        return -1;
    }
    
    buf[bytes_read] = '\0';
    return 0;
}

// Парсинг /proc/<pid>/stat
int parse_proc_stat(int pid, proc_stat_t *stat) {
    char path[64];
    char buf[MAX_BUF];
    
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    if (read_proc_file(path, buf, sizeof(buf)) == -1) {
        return -1;
    }
    
    // Парсим основные поля согласно формату /proc/<pid>/stat
    char *token = strtok(buf, " ");
    int field = 1;
    
    while (token != NULL) {
        switch (field) {
            case 1: stat->pid = atoi(token); break;
            case 2: 
                // Убираем скобки вокруг имени команды
                if (token[0] == '(') {
                    strncpy(stat->comm, token + 1, sizeof(stat->comm) - 1);
                    size_t len = strlen(stat->comm);
                    if (len > 0 && stat->comm[len - 1] == ')') {
                        stat->comm[len - 1] = '\0';
                    }
                }
                break;
            case 3: stat->state = token[0]; break;
            case 4: stat->ppid = atoi(token); break;
            case 14: stat->utime = strtoul(token, NULL, 10); break;
            case 15: stat->stime = strtoul(token, NULL, 10); break;
            case 20: stat->num_threads = atol(token); break;
        }
        token = strtok(NULL, " ");
        field++;
    }
    
    return 0;
}

// Парсинг /proc/<pid>/status
int parse_proc_status(int pid, proc_status_t *status) {
    char path[64];
    char buf[MAX_BUF];
    
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    if (read_proc_file(path, buf, sizeof(buf)) == -1) {
        return -1;
    }
    
    char *line = strtok(buf, "\n");
    while (line != NULL) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%lu", &status->vm_rss);
        } else if (strncmp(line, "RssAnon:", 8) == 0) {
            sscanf(line + 8, "%lu", &status->rss_anon);
        } else if (strncmp(line, "RssFile:", 8) == 0) {
            sscanf(line + 8, "%lu", &status->rss_file);
        } else if (strncmp(line, "voluntary_ctxt_switches:", 24) == 0) {
            sscanf(line + 24, "%lu", &status->voluntary_ctxt_switches);
        } else if (strncmp(line, "nonvoluntary_ctxt_switches:", 27) == 0) {
            sscanf(line + 27, "%lu", &status->nonvoluntary_ctxt_switches);
        }
        line = strtok(NULL, "\n");
    }
    
    return 0;
}

// Парсинг /proc/<pid>/io
int parse_proc_io(int pid, proc_io_t *io) {
    char path[64];
    char buf[MAX_BUF];
    
    snprintf(path, sizeof(path), "/proc/%d/io", pid);
    if (read_proc_file(path, buf, sizeof(buf)) == -1) {
        return -1;
    }
    
    char *line = strtok(buf, "\n");
    while (line != NULL) {
        if (strncmp(line, "read_bytes:", 11) == 0) {
            sscanf(line + 11, "%lu", &io->read_bytes);
        } else if (strncmp(line, "write_bytes:", 12) == 0) {
            sscanf(line + 12, "%lu", &io->write_bytes);
        }
        line = strtok(NULL, "\n");
    }
    
    return 0;
}

// Форматирование размера в человеко-читаемый вид
void format_size(unsigned long kb, char *buf, size_t buf_size) {
    if (kb >= 1024) {
        snprintf(buf, buf_size, "%.2f MiB", kb / 1024.0);
    } else {
        snprintf(buf, buf_size, "%lu KiB", kb);
    }
}

// Основная функция
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Использование: %s <pid>\n", argv[0]);
        return 1;
    }
    
    int pid = atoi(argv[1]);
    if (pid <= 0) {
        fprintf(stderr, "Ошибка: неверный PID\n");
        return 1;
    }
    
    proc_stat_t stat = {0};
    proc_status_t status = {0};
    proc_io_t io = {0};
    
    // Чтение и парсинг данных
    if (parse_proc_stat(pid, &stat) == -1) {
        fprintf(stderr, "Ошибка чтения /proc/%d/stat: %s\n", pid, strerror(errno));
        return 1;
    }
    
    if (parse_proc_status(pid, &status) == -1) {
        fprintf(stderr, "Ошибка чтения /proc/%d/status: %s\n", pid, strerror(errno));
        // Продолжаем выполнение, так как некоторые поля могут быть доступны
    }
    
    if (parse_proc_io(pid, &io) == -1) {
        fprintf(stderr, "Ошибка чтения /proc/%d/io: %s\n", pid, strerror(errno));
        // Продолжаем выполнение
    }
    
    // Вывод результатов
    printf("=== pstat для PID %d (%s) ===\n", pid, stat.comm);
    printf("PPid:                      %d\n", stat.ppid);
    printf("Threads:                   %ld\n", stat.num_threads);
    printf("State:                     %c\n", stat.state);
    
    // Время CPU
    double cpu_time_sec = (stat.utime + stat.stime) / (double)HZ;
    printf("CPU time (utime+stime):    %.2f сек\n", cpu_time_sec);
    printf("  utime:                   %lu тиков\n", stat.utime);
    printf("  stime:                   %lu тиков\n", stat.stime);
    
    // Переключения контекста
    printf("voluntary_ctxt_switches:   %lu\n", status.voluntary_ctxt_switches);
    printf("nonvoluntary_ctxt_switches:%lu\n", status.nonvoluntary_ctxt_switches);
    
    // Память
    char rss_buf[64], anon_buf[64], file_buf[64];
    format_size(status.vm_rss, rss_buf, sizeof(rss_buf));
    format_size(status.rss_anon, anon_buf, sizeof(anon_buf));
    format_size(status.rss_file, file_buf, sizeof(file_buf));
    
    printf("VmRSS:                     %s\n", rss_buf);
    printf("RssAnon:                   %s\n", anon_buf);
    printf("RssFile:                   %s\n", file_buf);
    
    // Ввод-вывод
    char read_buf[64], write_buf[64];
    format_size(io.read_bytes / 1024, read_buf, sizeof(read_buf));
    format_size(io.write_bytes / 1024, write_buf, sizeof(write_buf));
    
    printf("read_bytes:                %s\n", read_buf);
    printf("write_bytes:               %s\n", write_buf);
    
    return 0;
}