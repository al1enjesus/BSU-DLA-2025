#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define PATH_MAX_LEN 512
#define LINE_MAX_LEN 1024

typedef struct {
    int pid;
    int ppid;
    int threads;
    char state;
    unsigned long utime;
    unsigned long stime;
    long voluntary_ctxt_switches;
    long nonvoluntary_ctxt_switches;
    unsigned long vm_rss_kb;
    unsigned long rss_anon_kb;
    unsigned long rss_file_kb;
    unsigned long read_bytes;
    unsigned long write_bytes;
} proc_info_t;

static long get_clock_ticks_per_sec(void) {
    return sysconf(_SC_CLK_TCK);
}

static double ticks_to_seconds(unsigned long ticks) {
    return (double)ticks / get_clock_ticks_per_sec();
}

static void format_bytes(unsigned long bytes, char *buffer, size_t buf_size) {
    if (bytes >= (1UL << 30)) {
        snprintf(buffer, buf_size, "%.2f GiB", (double)bytes / (1UL << 30));
    } else if (bytes >= (1UL << 20)) {
        snprintf(buffer, buf_size, "%.2f MiB", (double)bytes / (1UL << 20));
    } else if (bytes >= (1UL << 10)) {
        snprintf(buffer, buf_size, "%.2f KiB", (double)bytes / (1UL << 10));
    } else {
        snprintf(buffer, buf_size, "%lu B", bytes);
    }
}

static int read_proc_stat(int pid, proc_info_t *info) {
    char path[PATH_MAX_LEN];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    
    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror("fopen /proc/pid/stat");
        return -1;
    }
    
    // Поля из /proc/pid/stat (нужны: ppid=4, state=3, utime=14, stime=15, threads=20)
    int ppid, threads;
    char state;
    unsigned long utime, stime;
    
    // Читаем нужные поля. Формат: pid (comm) state ppid ...
    if (fscanf(fp, "%*d %*s %c %d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu %*d %*d %*d %*d %d",
               &state, &ppid, &utime, &stime, &threads) != 5) {
        fprintf(stderr, "Failed to parse /proc/%d/stat\n", pid);
        fclose(fp);
        return -1;
    }
    
    info->ppid = ppid;
    info->state = state;
    info->utime = utime;
    info->stime = stime;
    info->threads = threads;
    
    fclose(fp);
    return 0;
}

static int read_proc_status(int pid, proc_info_t *info) {
    char path[PATH_MAX_LEN];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    
    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror("fopen /proc/pid/status");
        return -1;
    }
    
    char line[LINE_MAX_LEN];
    info->voluntary_ctxt_switches = -1;
    info->nonvoluntary_ctxt_switches = -1;
    info->vm_rss_kb = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line, "VmRSS: %lu kB", &info->vm_rss_kb);
        } else if (strncmp(line, "voluntary_ctxt_switches:", 24) == 0) {
            sscanf(line, "voluntary_ctxt_switches: %ld", &info->voluntary_ctxt_switches);
        } else if (strncmp(line, "nonvoluntary_ctxt_switches:", 27) == 0) {
            sscanf(line, "nonvoluntary_ctxt_switches: %ld", &info->nonvoluntary_ctxt_switches);
        }
    }
    
    fclose(fp);
    return 0;
}

static int read_proc_io(int pid, proc_info_t *info) {
    char path[PATH_MAX_LEN];
    snprintf(path, sizeof(path), "/proc/%d/io", pid);
    
    FILE *fp = fopen(path, "r");
    if (!fp) {
        // /proc/pid/io может быть недоступен для некоторых процессов
        info->read_bytes = 0;
        info->write_bytes = 0;
        return 0;
    }
    
    char line[LINE_MAX_LEN];
    info->read_bytes = 0;
    info->write_bytes = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "read_bytes:", 11) == 0) {
            sscanf(line, "read_bytes: %lu", &info->read_bytes);
        } else if (strncmp(line, "write_bytes:", 12) == 0) {
            sscanf(line, "write_bytes: %lu", &info->write_bytes);
        }
    }
    
    fclose(fp);
    return 0;
}

static int read_proc_smaps_rollup(int pid, proc_info_t *info) {
    char path[PATH_MAX_LEN];
    snprintf(path, sizeof(path), "/proc/%d/smaps_rollup", pid);
    
    FILE *fp = fopen(path, "r");
    if (!fp) {
        // smaps_rollup может быть недоступен на старых ядрах
        info->rss_anon_kb = 0;
        info->rss_file_kb = 0;
        return 0;
    }
    
    char line[LINE_MAX_LEN];
    info->rss_anon_kb = 0;
    info->rss_file_kb = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "RssAnon:", 8) == 0) {
            sscanf(line, "RssAnon: %lu kB", &info->rss_anon_kb);
        } else if (strncmp(line, "RssFile:", 8) == 0) {
            sscanf(line, "RssFile: %lu kB", &info->rss_file_kb);
        }
    }
    
    fclose(fp);
    return 0;
}

static void print_proc_info(const proc_info_t *info) {
    char rss_str[64], rss_anon_str[64], rss_file_str[64];
    char read_str[64], write_str[64];
    
    format_bytes(info->vm_rss_kb * 1024, rss_str, sizeof(rss_str));
    format_bytes(info->rss_anon_kb * 1024, rss_anon_str, sizeof(rss_anon_str));
    format_bytes(info->rss_file_kb * 1024, rss_file_str, sizeof(rss_file_str));
    format_bytes(info->read_bytes, read_str, sizeof(read_str));
    format_bytes(info->write_bytes, write_str, sizeof(write_str));
    
    double cpu_time_sec = ticks_to_seconds(info->utime + info->stime);
    
    printf("Process Information for PID %d:\n", info->pid);
    printf("================================\n");
    printf("PPid:                   %d\n", info->ppid);
    printf("Threads:                %d\n", info->threads);
    printf("State:                  %c\n", info->state);
    printf("CPU time (user):        %.3f sec (%lu ticks)\n", ticks_to_seconds(info->utime), info->utime);
    printf("CPU time (system):      %.3f sec (%lu ticks)\n", ticks_to_seconds(info->stime), info->stime);
    printf("CPU time (total):       %.3f sec\n", cpu_time_sec);
    printf("Voluntary ctx switches: %ld\n", info->voluntary_ctxt_switches);
    printf("Involuntary ctx switches: %ld\n", info->nonvoluntary_ctxt_switches);
    printf("VmRSS:                  %s (%.2f MiB)\n", rss_str, (double)info->vm_rss_kb / 1024.0);
    printf("RssAnon:                %s\n", rss_anon_str);
    printf("RssFile:                %s\n", rss_file_str);
    printf("Read bytes:             %s\n", read_str);
    printf("Write bytes:            %s\n", write_str);
    printf("\nClock ticks per second: %ld\n", get_clock_ticks_per_sec());
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
    
    proc_info_t info = {0};
    info.pid = pid;
    
    if (read_proc_stat(pid, &info) != 0) {
        return 1;
    }
    
    if (read_proc_status(pid, &info) != 0) {
        return 1;
    }
    
    read_proc_io(pid, &info);
    read_proc_smaps_rollup(pid, &info);
    
    print_proc_info(&info);
    
    return 0;
}