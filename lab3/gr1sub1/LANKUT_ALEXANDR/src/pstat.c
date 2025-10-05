#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/sysinfo.h>

void format_bytes(unsigned long bytes, char *output) {
    if (bytes > 1024 * 1024) {
        sprintf(output, "%.2f MiB", (float)bytes / (1024.0 * 1024.0));
    } else if (bytes > 1024) {
        sprintf(output, "%.2f KiB", (float)bytes / 1024.0);
    } else {
        sprintf(output, "%lu B", bytes);
    }
}

unsigned long read_status_value(FILE *fp, const char *field) {
    char line[256];
    char *value;
    unsigned long result = 0;

    rewind(fp);
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, field, strlen(field)) == 0) {
            value = strchr(line, ':');
            if (value) {
                result = strtoul(value + 1, NULL, 10);
                break;
            }
        }
    }
    return result;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Using: %s <pid>\n", argv[0]);
        return 1;
    }

    char path[256];
    char formatted[64];
    FILE *stat_fp, *status_fp, *io_fp;
    char state;
    int ppid, num_threads;
    unsigned long utime, stime;
    unsigned long vctx, nvctx;
    unsigned long vm_rss = 0, rss_anon = 0, rss_file = 0;
    unsigned long read_bytes = 0, write_bytes = 0;

    sprintf(path, "/proc/%s/stat", argv[1]);
    stat_fp = fopen(path, "r");
    if (!stat_fp) {
        perror("Error opening file stat");
        return 1;
    }

    if (fscanf(stat_fp, "%*d %*s %c %d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu",
           &state, &ppid, &utime, &stime) != 4) {
        fprintf(stderr, "Error reading file stat\n");
        fclose(stat_fp);
        return 1;
    }
    fclose(stat_fp);

    sprintf(path, "/proc/%s/status", argv[1]);
    status_fp = fopen(path, "r");
    if (!status_fp) {
        perror("Error opening file status");
        return 1;
    }

    num_threads = read_status_value(status_fp, "Threads");
    vm_rss = read_status_value(status_fp, "VmRSS");
    rss_anon = read_status_value(status_fp, "RssAnon");
    rss_file = read_status_value(status_fp, "RssFile");
    vctx = read_status_value(status_fp, "voluntary_ctxt_switches");
    nvctx = read_status_value(status_fp, "nonvoluntary_ctxt_switches");
    fclose(status_fp);

    sprintf(path, "/proc/%s/io", argv[1]);
    io_fp = fopen(path, "r");
    if (io_fp) {
        char line[256];
        while (fgets(line, sizeof(line), io_fp)) {
            if (strncmp(line, "read_bytes:", 11) == 0) {
                read_bytes = strtoul(line + 11, NULL, 10);
            } else if (strncmp(line, "write_bytes:", 12) == 0) {
                write_bytes = strtoul(line + 12, NULL, 10);
            }
        }
        fclose(io_fp);
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

    if (io_fp) {
        format_bytes(read_bytes, formatted);
        printf("Read: %s\n", formatted);
        format_bytes(write_bytes, formatted);
        printf("Written: %s\n", formatted);
    }
    printf("---------------------------\n");

    return 0;
}