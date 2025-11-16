#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#define MAX_BUF 1024

void trim(char* str) {
    if (!str) return;
    char* start = str;
    while (*start && isspace(*start)) start++;
    char* end = str + strlen(str) - 1;
    while (end > start && isspace(*end)) end--;
    memmove(str, start, end - start + 1);
    str[end - start + 1] = '\0';
}

int get_proc_value(const char* filename, const char* key, char* result) {
    FILE* f = fopen(filename, "r");
    if (!f) return 0;
    
    char buf[MAX_BUF];
    int found = 0;
    
    while (fgets(buf, sizeof(buf), f)) {
        if (strncmp(buf, key, strlen(key)) == 0) {
            char* val = strchr(buf, ':');
            if (val) {
                strcpy(result, val + 1);
                trim(result);
                found = 1;
                break;
            }
        }
    }
    
    fclose(f);
    return found;
}


int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <pid>\n", argv[0]);
        return 1;
    }
    
    char* pid = argv[1];
    long hz = sysconf(_SC_CLK_TCK);
    char stat_path[MAX_BUF];
    snprintf(stat_path, sizeof(stat_path), "/proc/%s/stat", pid);
    
    FILE* f = fopen(stat_path, "r");
    if (!f) {
        perror("Cannot open stat file");
        return 1;
    }
    
    char line[MAX_BUF];
    fgets(line, sizeof(line), f);
    fclose(f);

    char name[256], state;
    long ppid, utime, stime;
    
    if (sscanf(line, "%*d (%255[^)]) %c %ld %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %ld %ld",
               name, &state, &ppid, &utime, &stime) != 5) {
        fprintf(stderr, "Failed to parse stat\n");
        return 1;
    }
    
    char status_path[MAX_BUF], io_path[MAX_BUF], smaps_path[MAX_BUF];
    snprintf(status_path, sizeof(status_path), "/proc/%s/status", pid);
    snprintf(io_path, sizeof(io_path), "/proc/%s/io", pid);
    snprintf(smaps_path, sizeof(smaps_path), "/proc/%s/smaps_rollup", pid);
    
    char threads[64] = "N/A";
    char vmrss[64] = "N/A";
    char voluntary[64] = "N/A";
    char nonvoluntary[64] = "N/A";
    char read_bytes[64] = "N/A";
    char write_bytes[64] = "N/A";
    char rssanon[64] = "N/A";
    char rssfile[64] = "N/A";
    
    get_proc_value(status_path, "Threads", threads);
    get_proc_value(status_path, "VmRSS", vmrss);
    get_proc_value(status_path, "voluntary_ctxt_switches", voluntary);
    get_proc_value(status_path, "nonvoluntary_ctxt_switches", nonvoluntary);
    get_proc_value(io_path, "read_bytes", read_bytes);
    get_proc_value(io_path, "write_bytes", write_bytes);
    get_proc_value(smaps_path, "RssAnon", rssanon);
    get_proc_value(smaps_path, "RssFile", rssfile);
    
    printf("--- Process statistics for PID %s ---\n", pid);
    printf("PPid: %ld\n", ppid);
    printf("Threads: %s\n", threads);
    printf("State: %c\n", state);
    printf("utime: %ld ticks\n", utime);
    printf("stime: %ld ticks\n", stime);
    printf("CPU time: %.2f seconds\n", (double)(utime + stime) / hz);
    printf("voluntary_ctxt_switches: %s\n", voluntary);
    printf("nonvoluntary_ctxt_switches: %s\n", nonvoluntary);
    
    if (strcmp(vmrss, "N/A") != 0 && strlen(vmrss) > 0) {
        char* kb_pos = strstr(vmrss, " kB");
        if (kb_pos) *kb_pos = '\0';
        trim(vmrss);
        
        if (strlen(vmrss) > 0) {
            long rss_kb = atol(vmrss);
            printf("VmRSS: %ld kB\n", rss_kb);
            printf("RSS MiB: %.2f MiB\n", rss_kb / 1024.0);
        } else {
            printf("VmRSS: N/A\n");
            printf("RSS MiB: N/A\n");
        }
    } else {
        printf("VmRSS: N/A\n");
        printf("RSS MiB: N/A\n");
    }
    
    printf("RssAnon: %s\n", rssanon);
    printf("RssFile: %s\n", rssfile);
    
    if (strcmp(read_bytes, "N/A") != 0 && strlen(read_bytes) > 0) {
        long long bytes = atoll(read_bytes);
        printf("read_bytes: %lld bytes", bytes);
        if (bytes >= 1024*1024) printf(" (%.2f MiB)", bytes/(1024.0*1024.0));
        else if (bytes >= 1024) printf(" (%.2f KiB)", bytes/1024.0);
        printf("\n");
    } else {
        printf("read_bytes: N/A\n");
    }
    
    if (strcmp(write_bytes, "N/A") != 0 && strlen(write_bytes) > 0) {
        long long bytes = atoll(write_bytes);
        printf("write_bytes: %lld bytes", bytes);
        if (bytes >= 1024*1024) printf(" (%.2f MiB)", bytes/(1024.0*1024.0));
        else if (bytes >= 1024) printf(" (%.2f KiB)", bytes/1024.0);
        printf("\n");
    } else {
        printf("write_bytes: N/A\n");
    }
    
    return 0;
}
