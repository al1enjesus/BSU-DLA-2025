#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>

int validate_pid(const char *pid_str) {
    if (!pid_str || !*pid_str) {
        return 0;
    }

    for (const char *p = pid_str; *p; p++) {
        if (!isdigit(*p)) {
            return 0;
        }
    }

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/proc/%s", pid_str);
    
    if (access(path, F_OK) == -1) {
        return 0;
    }

    return 1;
}

void format_bytes(unsigned long bytes, char *output) {
    if (bytes > 1024 * 1024) {
        snprintf(output, 64, "%.2f MiB", (float)bytes / (1024.0 * 1024.0));
    } else if (bytes > 1024) {
        snprintf(output, 64, "%.2f KiB", (float)bytes / 1024.0);
    } else {
        snprintf(output, 64, "%lu B", bytes);
    }
}

unsigned long read_status_value(FILE *fp, const char *field) {
    char line[PATH_MAX];
    char *value;
    unsigned long result = 0;
    long pos;

    pos = ftell(fp);
    if (pos == -1) {
        return 0;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, field, strlen(field)) == 0) {
            value = strchr(line, ':');
            if (value) {
                char *endptr;
                errno = 0;
                result = strtoul(value + 1, &endptr, 10);
                if (errno || endptr == value + 1) {
                    result = 0;
                }
                break;
            }
        }
    }

    if (fseek(fp, pos, SEEK_SET) == -1) {
        perror("Failed to reset file position");
    }
    return result;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <pid>\n", argv[0]);
        return 1;
    }

    if (!validate_pid(argv[1])) {
        fprintf(stderr, "Error: Invalid PID or process does not exist\n");
        return 1;
    }

    char path[PATH_MAX];
    char formatted[64];
    FILE *stat_fp, *status_fp, *io_fp;
    char state;
    int ppid, num_threads;
    unsigned long utime, stime;
    unsigned long vctx, nvctx;
    unsigned long vm_rss = 0, rss_anon = 0, rss_file = 0;
    unsigned long read_bytes = 0, write_bytes = 0;
    int have_io_stats = 0;

    snprintf(path, sizeof(path), "/proc/%s/stat", argv[1]);
    stat_fp = fopen(path, "r");
    if (!stat_fp) {
        perror("Failed to open stat file");
        return 1;
    }

    int scan_result = fscanf(stat_fp, "%*d %*s %c %d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu",
           &state, &ppid, &utime, &stime);
    fclose(stat_fp);

    if (scan_result != 4) {
        fprintf(stderr, "Error reading from stat file (expected 4 values, got %d)\n", scan_result);
        return 1;
    }

    snprintf(path, sizeof(path), "/proc/%s/status", argv[1]);
    status_fp = fopen(path, "r");
    if (!status_fp) {
        perror("Failed to open status file");
        return 1;
    }

    num_threads = read_status_value(status_fp, "Threads");
    vm_rss = read_status_value(status_fp, "VmRSS");
    rss_anon = read_status_value(status_fp, "RssAnon");
    rss_file = read_status_value(status_fp, "RssFile");
    vctx = read_status_value(status_fp, "voluntary_ctxt_switches");
    nvctx = read_status_value(status_fp, "nonvoluntary_ctxt_switches");
    fclose(status_fp);

    snprintf(path, sizeof(path), "/proc/%s/io", argv[1]);
    io_fp = fopen(path, "r");
    if (io_fp) {
        char line[PATH_MAX];
        char *endptr;
        while (fgets(line, sizeof(line), io_fp)) {
            if (strncmp(line, "read_bytes:", 11) == 0) {
                errno = 0;
                read_bytes = strtoul(line + 11, &endptr, 10);
                if (errno || endptr == line + 11) {
                    read_bytes = 0;
                }
            } else if (strncmp(line, "write_bytes:", 12) == 0) {
                errno = 0;
                write_bytes = strtoul(line + 12, &endptr, 10);
                if (errno || endptr == line + 12) {
                    write_bytes = 0;
                }
            }
        }
        fclose(io_fp);
        have_io_stats = 1;
    }

    printf("\nProcess %s information:\n", argv[1]);
    printf("---------------------------\n");
    printf("Parent PID: %d\n", ppid);
    printf("Thread count: %d\n", num_threads);
    printf("State: %c", state);
    switch(state) {
        case 'R': printf(" (running)\n"); break;
        case 'S': printf(" (sleeping)\n"); break;
        case 'D': printf(" (disk sleep)\n"); break;
        case 'Z': printf(" (zombie)\n"); break;
        case 'T': printf(" (stopped)\n"); break;
        default: printf("\n");
    }

    long clock_ticks = sysconf(_SC_CLK_TCK);
    if (clock_ticks <= 0) {
        fprintf(stderr, "Warning: Could not determine clock ticks per second\n");
        clock_ticks = 100;  // Используем типичное значение как запасной вариант
    }
    double cpu_time = (double)(utime + stime) / clock_ticks;
    printf("CPU Time: %.2f sec (user: %lu, system: %lu)\n", 
           cpu_time, utime, stime);

    printf("Context switches: voluntary=%lu, nonvoluntary=%lu\n", 
           vctx, nvctx);

    format_bytes(vm_rss * 1024, formatted);
    printf("Memory usage (VmRSS): %s\n", formatted);
    format_bytes(rss_anon * 1024, formatted);
    printf("Anonymous memory (RssAnon): %s\n", formatted);
    format_bytes(rss_file * 1024, formatted);
    printf("File cache (RssFile): %s\n", formatted);

    if (have_io_stats) {
        format_bytes(read_bytes, formatted);
        printf("Read: %s\n", formatted);
        format_bytes(write_bytes, formatted);
        printf("Written: %s\n", formatted);
    }
    printf("---------------------------\n");

    return 0;
}