#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define HZ sysconf(_SC_CLK_TCK)

void format_size(long long bytes, char *buf, size_t bufsize) {
    if (bytes < 1024) snprintf(buf, bufsize, "%lld B", bytes);
    else if (bytes < 1024*1024) snprintf(buf, bufsize, "%.2f KiB", bytes/1024.0);
    else snprintf(buf, bufsize, "%.2f MiB", bytes/1024.0/1024.0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <pid>\n", argv[0]);
        return 1;
    }

    char *endptr;
    long pid = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0' || pid <= 0) {
        fprintf(stderr, "Invalid PID: %s\n", argv[1]);
        return 1;
    }

    char path[256], buf[1024];

    snprintf(path, sizeof(path), "/proc/%ld/stat", pid);
    FILE *fstat = fopen(path, "r");
    if (!fstat) { perror("fopen stat"); return 1; }

    long utime=0, stime=0;
    char comm[256], state;
    int ppid=0;
    if (fscanf(fstat, "%*d (%255[^)]) %c %d %*d %*d %*d %*d %*u %*lu %*lu %*lu %*lu %ld %ld",
               comm, &state, &ppid, &utime, &stime) != 5) {
        fprintf(stderr, "Failed to parse stat\n");
        fclose(fstat);
        return 1;
    }
    fclose(fstat);

    snprintf(path, sizeof(path), "/proc/%ld/status", pid);
    FILE *fstatus = fopen(path, "r");
    if (!fstatus) { perror("fopen status"); return 1; }

    int threads=0;
    long voluntary_ctxt_switches=0, nonvoluntary_ctxt_switches=0, vmrss_kb=0;
    while (fgets(buf, sizeof(buf), fstatus)) {
        if (sscanf(buf, "Threads: %d", &threads) == 1) continue;
        if (sscanf(buf, "VmRSS: %ld kB", &vmrss_kb) == 1) continue;
        if (sscanf(buf, "voluntary_ctxt_switches: %ld", &voluntary_ctxt_switches) == 1) continue;
        if (sscanf(buf, "nonvoluntary_ctxt_switches: %ld", &nonvoluntary_ctxt_switches) == 1) continue;
    }
    fclose(fstatus);

    snprintf(path, sizeof(path), "/proc/%ld/io", pid);
    FILE *fio = fopen(path, "r");
    long long read_bytes=0, write_bytes=0;
    if (fio) {
        while (fgets(buf, sizeof(buf), fio)) {
            sscanf(buf, "read_bytes: %lld", &read_bytes);
            sscanf(buf, "write_bytes: %lld", &write_bytes);
        }
        fclose(fio);
    }

    // --- /proc/<pid>/smaps_rollup (VmAnon, VmFile) ---
    snprintf(path, sizeof(path), "/proc/%ld/smaps_rollup", pid);
    FILE *fsmaps = fopen(path, "r");
    long rss_anon=0, rss_file=0;
    if (fsmaps) {
        while (fgets(buf, sizeof(buf), fsmaps)) {
            sscanf(buf, "RssAnon: %ld kB", &rss_anon);
            sscanf(buf, "RssFile: %ld kB", &rss_file);
        }
        fclose(fsmaps);
    }

    char str_vmrss[32], str_rss_anon[32], str_rss_file[32];
    format_size(vmrss_kb*1024LL, str_vmrss, sizeof(str_vmrss));
    format_size(rss_anon*1024LL, str_rss_anon, sizeof(str_rss_anon));
    format_size(rss_file*1024LL, str_rss_file, sizeof(str_rss_file));

    printf("=== pstat summary for PID %ld ===\n", pid);
    printf("Command: %s\n", comm);
    printf("PPid: %d\n", ppid);
    printf("Threads: %d\n", threads);
    printf("State: %c\n", state);
    printf("User time: %.3f s\n", utime/(double)HZ);
    printf("System time: %.3f s\n", stime/(double)HZ);
    printf("Total CPU time: %.3f s\n", (utime+stime)/(double)HZ);
    printf("VmRSS: %s\n", str_vmrss);
    printf("RssAnon: %s\n", str_rss_anon);
    printf("RssFile: %s\n", str_rss_file);
    if (read_bytes || write_bytes)
        printf("Read bytes: %lld\nWrite bytes: %lld\n", read_bytes, write_bytes);
    printf("Voluntary ctxt switches: %ld\n", voluntary_ctxt_switches);
    printf("Nonvoluntary ctxt switches: %ld\n", nonvoluntary_ctxt_switches);

    return 0;
}
