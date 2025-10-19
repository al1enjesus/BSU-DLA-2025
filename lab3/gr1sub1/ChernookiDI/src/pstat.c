#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_LINE 1024

long get_hz() {
    return sysconf(_SC_CLK_TCK);
}

void format_bytes(unsigned long bytes, char *buffer, size_t bufsize) {
    if (bytes >= 1024 * 1024) {
        snprintf(buffer, bufsize, "%.2f MiB", (double)bytes / (1024 * 1024));
    } else if (bytes >= 1024) {
        snprintf(buffer, bufsize, "%.2f KiB", (double)bytes / 1024);
    } else {
        snprintf(buffer, bufsize, "%lu B", bytes);
    }
}

int read_stat(int pid, unsigned long *utime, unsigned long *stime, int *ppid, char *state) {
    char path[256];
    FILE *fp;
    char line[MAX_LINE];
    
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    fp = fopen(path, "r");
    if (!fp) {
        perror("Cannot open stat file");
        return -1;
    }
    
    if (fgets(line, sizeof(line), fp)) {
        char *token = strtok(line, " ");
        int field = 1;
        
        while (token && field <= 15) {
            switch (field) {
                case 3:
                    *state = token[0];
                    break;
                case 4:
                    *ppid = atoi(token);
                    break;
                case 14:
                    *utime = strtoul(token, NULL, 10);
                    break;
                case 15:
                    *stime = strtoul(token, NULL, 10);
                    break;
            }
            token = strtok(NULL, " ");
            field++;
        }
    }
    
    fclose(fp);
    return 0;
}

int read_status(int pid, int *threads, unsigned long *vmrss, unsigned long *voluntary_switches, unsigned long *nonvoluntary_switches) {
    char path[256];
    FILE *fp;
    char line[MAX_LINE];
    
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    fp = fopen(path, "r");
    if (!fp) {
        perror("Cannot open status file");
        return -1;
    }
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Threads:", 8) == 0) {
            sscanf(line, "Threads:\t%d", threads);
        } else if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line, "VmRSS:\t%lu kB", vmrss);
            *vmrss *= 1024;
        } else if (strncmp(line, "voluntary_ctxt_switches:", 24) == 0) {
            sscanf(line, "voluntary_ctxt_switches:\t%lu", voluntary_switches);
        } else if (strncmp(line, "nonvoluntary_ctxt_switches:", 27) == 0) {
            sscanf(line, "nonvoluntary_ctxt_switches:\t%lu", nonvoluntary_switches);
        }
    }
    
    fclose(fp);
    return 0;
}

int read_smaps_rollup(int pid, unsigned long *rss_anon, unsigned long *rss_file) {
    char path[256];
    FILE *fp;
    char line[MAX_LINE];
    
    snprintf(path, sizeof(path), "/proc/%d/smaps_rollup", pid);
    fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "RssAnon:", 8) == 0) {
            sscanf(line, "RssAnon:\t%lu kB", rss_anon);
            *rss_anon *= 1024;
        } else if (strncmp(line, "RssFile:", 8) == 0) {
            sscanf(line, "RssFile:\t%lu kB", rss_file);
            *rss_file *= 1024;
        }
    }
    
    fclose(fp);
    return 0;
}

int read_io(int pid, unsigned long *read_bytes, unsigned long *write_bytes) {
    char path[256];
    FILE *fp;
    char line[MAX_LINE];
    
    snprintf(path, sizeof(path), "/proc/%d/io", pid);
    fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "read_bytes:", 11) == 0) {
            sscanf(line, "read_bytes: %lu", read_bytes);
        } else if (strncmp(line, "write_bytes:", 12) == 0) {
            sscanf(line, "write_bytes: %lu", write_bytes);
        }
    }
    
    fclose(fp);
    return 0;
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
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d", pid);
    struct stat st;
    if (stat(path, &st) != 0) {
        fprintf(stderr, "Process %d not found\n", pid);
        return 1;
    }
    
    unsigned long utime = 0, stime = 0;
    int ppid = 0, threads = 0;
    char state = '?';
    unsigned long vmrss = 0, voluntary_switches = 0, nonvoluntary_switches = 0;
    unsigned long rss_anon = 0, rss_file = 0;
    unsigned long read_bytes = 0, write_bytes = 0;
    
    long hz = get_hz();
    
    if (read_stat(pid, &utime, &stime, &ppid, &state) != 0) {
        return 1;
    }
    
    if (read_status(pid, &threads, &vmrss, &voluntary_switches, &nonvoluntary_switches) != 0) {
        return 1;
    }
    
    int has_smaps = read_smaps_rollup(pid, &rss_anon, &rss_file) == 0;
    int has_io = read_io(pid, &read_bytes, &write_bytes) == 0;
    
    char vmrss_str[64], rss_anon_str[64], rss_file_str[64];
    char read_str[64], write_str[64];
    
    format_bytes(vmrss, vmrss_str, sizeof(vmrss_str));
    format_bytes(rss_anon, rss_anon_str, sizeof(rss_anon_str));
    format_bytes(rss_file, rss_file_str, sizeof(rss_file_str));
    format_bytes(read_bytes, read_str, sizeof(read_str));
    format_bytes(write_bytes, write_str, sizeof(write_str));
    
    double cpu_time_sec = (double)(utime + stime) / hz;
    
    printf("Process Statistics for PID %d:\n", pid);
    printf("=================================\n");
    printf("PPid:                    %d\n", ppid);
    printf("Threads:                 %d\n", threads);
    printf("State:                   %c\n", state);
    printf("utime (user ticks):      %lu\n", utime);
    printf("stime (kernel ticks):    %lu\n", stime);
    printf("CPU time (seconds):      %.2f\n", cpu_time_sec);
    printf("HZ (ticks per second):   %ld\n", hz);
    printf("Voluntary ctx switches:  %lu\n", voluntary_switches);
    printf("Nonvoluntary ctx switches: %lu\n", nonvoluntary_switches);
    printf("VmRSS:                   %s\n", vmrss_str);
    
    if (has_smaps) {
        printf("RssAnon:                 %s\n", rss_anon_str);
        printf("RssFile:                 %s\n", rss_file_str);
    } else {
        printf("RssAnon:                 N/A (smaps_rollup not available)\n");
        printf("RssFile:                 N/A (smaps_rollup not available)\n");
    }
    
    if (has_io) {
        printf("Read bytes:              %s\n", read_str);
        printf("Write bytes:             %s\n", write_str);
    } else {
        printf("Read bytes:              N/A (io not available or need root)\n");
        printf("Write bytes:             N/A (io not available or need root)\n");
    }
    
    return 0;
}