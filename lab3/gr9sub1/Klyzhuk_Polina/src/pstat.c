#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE 4096

void print_human_readable_kb(long kb) {
    if (kb > 1024) {
        printf("%.2f MiB", (double)kb / 1024.0);
    } else {
        printf("%ld KiB", kb);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <pid>\n", argv[0]);
        return 1;
    }

    char path[256], buf[BUF_SIZE];
    FILE *f;
    int pid = atoi(argv[1]);

    // -------- /proc/<pid>/stat --------
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    f = fopen(path, "r");
    if (!f) { perror("stat"); return 1; }

    int ppid;
    char comm[64], state;
    unsigned long utime, stime;
    fscanf(f, "%*d %s %c %d", comm, &state, &ppid);

    // Пропустить лишние поля до utime(14), stime(15)
    for (int i = 0; i < 10; i++) fscanf(f, "%*s");
    fscanf(f, "%lu %lu", &utime, &stime);
    fclose(f);

    long hz = sysconf(_SC_CLK_TCK);
    double cpu_time = (utime + stime) / (double)hz;

    // -------- /proc/<pid>/status --------
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    f = fopen(path, "r");
    if (!f) { perror("status"); return 1; }

    long vmrss = 0, rssanon = 0, rssfile = 0;
    int threads = 0;
    long voluntary = 0, nonvoluntary = 0;
    while (fgets(buf, sizeof(buf), f)) {
        if (sscanf(buf, "Threads: %d", &threads) == 1) continue;
        if (sscanf(buf, "VmRSS: %ld", &vmrss) == 1) continue;
        if (sscanf(buf, "RssAnon: %ld", &rssanon) == 1) continue;
        if (sscanf(buf, "RssFile: %ld", &rssfile) == 1) continue;
        if (sscanf(buf, "voluntary_ctxt_switches: %ld", &voluntary) == 1) continue;
        if (sscanf(buf, "nonvoluntary_ctxt_switches: %ld", &nonvoluntary) == 1) continue;
    }
    fclose(f);

    // -------- /proc/<pid>/io --------
    snprintf(path, sizeof(path), "/proc/%d/io", pid);
    f = fopen(path, "r");
    if (!f) { perror("io"); return 1; }

    long read_bytes = 0, write_bytes = 0;
    while (fgets(buf, sizeof(buf), f)) {
        sscanf(buf, "read_bytes: %ld", &read_bytes);
        sscanf(buf, "write_bytes: %ld", &write_bytes);
    }
    fclose(f);

    // -------- Вывод --------
    printf("Process %d (%s)\n", pid, comm);
    printf("  PPid: %d\n", ppid);
    printf("  State: %c\n", state);
    printf("  Threads: %d\n", threads);
    printf("  CPU time: %.2f sec (utime+stime)\n", cpu_time);
    printf("  Context switches: voluntary=%ld, nonvoluntary=%ld\n", voluntary, nonvoluntary);

    printf("  Memory:\n");
    printf("    VmRSS: "); print_human_readable_kb(vmrss); printf("\n");
    printf("    RssAnon: "); print_human_readable_kb(rssanon); printf("\n");
    printf("    RssFile: "); print_human_readable_kb(rssfile); printf("\n");

    printf("  IO:\n");
    printf("    read_bytes: %ld\n", read_bytes);
    printf("    write_bytes: %ld\n", write_bytes);

    return 0;
}
