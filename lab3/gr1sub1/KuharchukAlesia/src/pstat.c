#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_BUF 4096
#define HZ 100  // Частота системного таймера

typedef struct {
    int pid;
    char comm[256];
    char state;
    int ppid;
    unsigned long utime;
    unsigned long stime;
    long num_threads;
} proc_stat_t;

typedef struct {
    unsigned long vm_rss;
    unsigned long rss_anon;
    unsigned long rss_file;
    unsigned long voluntary_ctxt_switches;
    unsigned long nonvoluntary_ctxt_switches;
} proc_status_t;

typedef struct {
    unsigned long read_bytes;
    unsigned long write_bytes;
} proc_io_t;

int read_proc_file(const char* filename, char* buffer, size_t size) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        return -1;
    }
    
    size_t bytes_read = fread(buffer, 1, size - 1, file);
    buffer[bytes_read] = '\0';
    fclose(file);
    
    return 0;
}

int parse_proc_stat(pid_t pid, proc_stat_t* stat) {
    char filename[64];
    char buffer[MAX_BUF];
    
    snprintf(filename, sizeof(filename), "/proc/%d/stat", pid);
    
    if (read_proc_file(filename, buffer, sizeof(buffer)) < 0) {
        return -1;
    }
    
    sscanf(buffer, 
           "%d %s %c %d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu %*d %*d %*d %*d %*d %*d %*u %*u %ld",
           &stat->pid, stat->comm, &stat->state, &stat->ppid,
           &stat->utime, &stat->stime, &stat->num_threads);
    
    return 0;
}

int parse_proc_status(pid_t pid, proc_status_t* status) {
    char filename[64];
    char buffer[MAX_BUF];
    char line[256];
    
    snprintf(filename, sizeof(filename), "/proc/%d/status", pid);
    
    if (read_proc_file(filename, buffer, sizeof(buffer)) < 0) {
        return -1;
    }
    
    // Ищем нужные поля в файле status
    char* pos = buffer;
    while (*pos) {
        if (strncmp(pos, "VmRSS:", 6) == 0) {
            sscanf(pos + 6, "%lu", &status->vm_rss);
        } else if (strncmp(pos, "RssAnon:", 8) == 0) {
            sscanf(pos + 8, "%lu", &status->rss_anon);
        } else if (strncmp(pos, "RssFile:", 8) == 0) {
            sscanf(pos + 8, "%lu", &status->rss_file);
        } else if (strncmp(pos, "voluntary_ctxt_switches:", 24) == 0) {
            sscanf(pos + 24, "%lu", &status->voluntary_ctxt_switches);
        } else if (strncmp(pos, "nonvoluntary_ctxt_switches:", 27) == 0) {
            sscanf(pos + 27, "%lu", &status->nonvoluntary_ctxt_switches);
        }
        pos = strchr(pos, '\n');
        if (!pos) break;
        pos++;
    }
    
    return 0;
}

int parse_proc_io(pid_t pid, proc_io_t* io) {
    char filename[64];
    char buffer[MAX_BUF];
    char line[256];
    
    snprintf(filename, sizeof(filename), "/proc/%d/io", pid);
    
    if (read_proc_file(filename, buffer, sizeof(buffer)) < 0) {
        return -1;
    }
    
    char* pos = buffer;
    while (*pos) {
        if (strncmp(pos, "read_bytes:", 11) == 0) {
            sscanf(pos + 11, "%lu", &io->read_bytes);
        } else if (strncmp(pos, "write_bytes:", 12) == 0) {
            sscanf(pos + 12, "%lu", &io->write_bytes);
        }
        pos = strchr(pos, '\n');
        if (!pos) break;
        pos++;
    }
    
    return 0;
}

void format_memory_size(unsigned long kb, char* buffer, size_t size) {
    if (kb >= 1024) {
        snprintf(buffer, size, "%.2f MiB", kb / 1024.0);
    } else {
        snprintf(buffer, size, "%lu KiB", kb);
    }
}

void format_io_size(unsigned long bytes, char* buffer, size_t size) {
    if (bytes >= 1024 * 1024) {
        snprintf(buffer, size, "%.2f MiB", bytes / (1024.0 * 1024.0));
    } else if (bytes >= 1024) {
        snprintf(buffer, size, "%.2f KiB", bytes / 1024.0);
    } else {
        snprintf(buffer, size, "%lu bytes", bytes);
    }
}
int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <pid>\n", argv[0]);
        return 1;
    }
    
    pid_t pid = atoi(argv[1]);
    
    proc_stat_t stat;
    proc_status_t status;
    proc_io_t io;
    
    // Читаем данные из /proc
    if (parse_proc_stat(pid, &stat) < 0) {
        fprintf(stderr, "Error reading /proc/%d/stat\n", pid);
        return 1;
    }
    
    if (parse_proc_status(pid, &status) < 0) {
        fprintf(stderr, "Error reading /proc/%d/status\n", pid);
        return 1;
    }
    
    if (parse_proc_io(pid, &io) < 0) {
        fprintf(stderr, "Warning: Cannot read /proc/%d/io\n", pid);
        memset(&io, 0, sizeof(io));
    }
    
    // Форматируем вывод
    char rss_mib[64], rss_anon_fmt[64], rss_file_fmt[64];
    char read_bytes_fmt[64], write_bytes_fmt[64];
    
    format_memory_size(status.vm_rss, rss_mib, sizeof(rss_mib));
    format_memory_size(status.rss_anon, rss_anon_fmt, sizeof(rss_anon_fmt));
    format_memory_size(status.rss_file, rss_file_fmt, sizeof(rss_file_fmt));
    format_io_size(io.read_bytes, read_bytes_fmt, sizeof(read_bytes_fmt));
    format_io_size(io.write_bytes, write_bytes_fmt, sizeof(write_bytes_fmt));
    
    double cpu_time_sec = (stat.utime + stat.stime) / (double)HZ;
    
    // Выводим сводку
    printf("Process %d (%s) statistics:\n", pid, stat.comm);
    printf("========================================\n");
    printf("Basic Information:\n");
    printf("  PPid: %d\n", stat.ppid);
    printf("  Threads: %ld\n", stat.num_threads);
    printf("  State: %c\n", stat.state);
    printf("  CPU time: %.2f sec (utime: %lu, stime: %lu)\n", 
           cpu_time_sec, stat.utime, stat.stime);
    
    printf("\nMemory Information:\n");
    printf("  VmRSS: %s\n", rss_mib);
    printf("  RssAnon: %s\n", rss_anon_fmt);
    printf("  RssFile: %s\n", rss_file_fmt);
    
    printf("\nContext Switches:\n");
    printf("  Voluntary: %lu\n", status.voluntary_ctxt_switches);
    printf("  Non-voluntary: %lu\n", status.nonvoluntary_ctxt_switches);
    
    printf("\nI/O Information:\n");
    printf("  Read bytes: %s\n", read_bytes_fmt);
    printf("  Write bytes: %s\n", write_bytes_fmt);
    
    return 0;
}
