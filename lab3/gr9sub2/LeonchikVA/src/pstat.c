#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/types.h>

static char *read_file_all(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long len = ftell(f);
    if (len < 0) { fclose(f); return NULL; }
    rewind(f);
    char *buf = malloc((size_t)len + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)len, f);
    buf[r] = '\0';
    fclose(f);
    return buf;
}

static void format_bytes(unsigned long long bytes, char *out, size_t outlen) {
    const char *units[] = {"B","KiB","MiB","GiB","TiB"};
    double v = (double)bytes;
    int u = 0;
    while (v >= 1024.0 && u < 4) { v /= 1024.0; u++; }
    snprintf(out, outlen, "%.2f %s", v, units[u]);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <pid>\n", argv[0]);
        return 2;
    }
    const char *pid = argv[1];
    char path[256];

    /* --------------------- /proc/<pid>/stat --------------------- */
    snprintf(path, sizeof(path), "/proc/%s/stat", pid);
    FILE *fs = fopen(path, "r");
    if (!fs) {
        fprintf(stderr, "Error: cannot open %s: %s\n", path, strerror(errno));
        return 1;
    }
    char statbuf[8192];
    if (!fgets(statbuf, sizeof(statbuf), fs)) {
        fclose(fs);
        fprintf(stderr, "Error: cannot read %s\n", path);
        return 1;
    }
    fclose(fs);

    /* parse comm and state via sscanf for safety */
    int parsed_pid = 0;
    char comm[512] = {0};
    char state_char = '?';
    if (sscanf(statbuf, "%d (%511[^)]) %c", &parsed_pid, comm, &state_char) < 3) {
        /* fallback: try finding ')' */
        char *rp = strrchr(statbuf, ')');
        if (rp && rp[1]) state_char = rp[2];
    }

    /* get tokens after the closing ')' */
    char *rparen = strrchr(statbuf, ')');
    if (!rparen) {
        fprintf(stderr, "Error: unexpected format in %s\n", path);
        return 1;
    }
    char *rest = rparen + 2; // skip ") "

    /* tokenize rest to extract fields by index */
    #define MAXTOK 128
    char *tokbuf = strdup(rest);
    if (!tokbuf) { fprintf(stderr, "malloc failed\n"); return 1; }
    char *tokens[MAXTOK];
    int tcount = 0;
    char *saveptr = NULL;
    char *t = strtok_r(tokbuf, " ", &saveptr);
    while (t && tcount < MAXTOK) {
        tokens[tcount++] = t;
        t = strtok_r(NULL, " ", &saveptr);
    }
    long ppid_stat = -1;
    unsigned long long utime = 0, stime = 0;
    long num_threads_stat = -1;
    if (tcount > 1) ppid_stat = atol(tokens[1]);   // field 4
    if (tcount > 11) utime = strtoull(tokens[11], NULL, 10); // field 14
    if (tcount > 12) stime = strtoull(tokens[12], NULL, 10); // field 15
    if (tcount > 17) num_threads_stat = atol(tokens[17]); // field 20
    free(tokbuf);

    /* --------------------- /proc/<pid>/status --------------------- */
    snprintf(path, sizeof(path), "/proc/%s/status", pid);
    FILE *fst = fopen(path, "r");
    long ppid_status = -1;
    long threads_status = -1;
    char state_status[128] = "-";
    long vmrss_kb = -1;
    unsigned long long voluntary_ctxt = 0, nonvoluntary_ctxt = 0;
    if (fst) {
        char *line = NULL;
        size_t lcap = 0;
        ssize_t linelen;
        while ((linelen = getline(&line, &lcap, fst)) > 0) {
            if (strncmp(line, "PPid:", 5) == 0) {
                sscanf(line+5, "%ld", &ppid_status);
            } else if (strncmp(line, "Threads:", 8) == 0) {
                sscanf(line+8, "%ld", &threads_status);
            } else if (strncmp(line, "State:", 6) == 0) {
                char *p = line + 6;
                while (*p == ' ' || *p == '\t') p++;
                strncpy(state_status, p, sizeof(state_status)-1);
                /* trim newline */
                char *nl = strchr(state_status, '\n');
                if (nl) *nl = '\0';
            } else if (strncmp(line, "VmRSS:", 6) == 0) {
                sscanf(line+6, "%ld", &vmrss_kb);
            } else if (strncmp(line, "voluntary_ctxt_switches:", 24) == 0) {
                sscanf(line+24, "%llu", &voluntary_ctxt);
            } else if (strncmp(line, "nonvoluntary_ctxt_switches:", 27) == 0) {
                sscanf(line+27, "%llu", &nonvoluntary_ctxt);
            }
        }
        free(line);
        fclose(fst);
    } else {
        /* no status (possible for short-lived processes or permission) */
        /* keep defaults */
    }

    /* --------------------- /proc/<pid>/io --------------------- */
    snprintf(path, sizeof(path), "/proc/%s/io", pid);
    FILE *fio = fopen(path, "r");
    unsigned long long read_bytes = 0, write_bytes = 0;
    if (fio) {
        char line[256];
        while (fgets(line, sizeof(line), fio)) {
            if (strncmp(line, "read_bytes:", 11) == 0) {
                sscanf(line+11, "%llu", &read_bytes);
            } else if (strncmp(line, "write_bytes:", 12) == 0) {
                sscanf(line+12, "%llu", &write_bytes);
            }
        }
        fclose(fio);
    }

    /* --------------------- /proc/<pid>/smaps_rollup (optional) --------------------- */
    snprintf(path, sizeof(path), "/proc/%s/smaps_rollup", pid);
    FILE *fsm = fopen(path, "r");
    long rss_anon_kb = -1, rss_file_kb = -1;
    if (fsm) {
        char line[256];
        while (fgets(line, sizeof(line), fsm)) {
            if (strncmp(line, "RssAnon:", 8) == 0) {
                sscanf(line+8, "%ld", &rss_anon_kb);
            } else if (strncmp(line, "RssFile:", 8) == 0) {
                sscanf(line+8, "%ld", &rss_file_kb);
            }
        }
        fclose(fsm);
    }

    /* --------------------- calculations and print --------------------- */
    long hz = sysconf(_SC_CLK_TCK);
    unsigned long long total_ticks = utime + stime;
    double cpu_seconds = hz > 0 ? (double)total_ticks / (double)hz : 0.0;

    char read_fmt[64], write_fmt[64], vmrss_fmt[64];
    format_bytes(read_bytes, read_fmt, sizeof(read_fmt));
    format_bytes(write_bytes, write_fmt, sizeof(write_fmt));
    if (vmrss_kb >= 0) {
        format_bytes((unsigned long long)vmrss_kb * 1024ULL, vmrss_fmt, sizeof(vmrss_fmt));
    } else {
        strncpy(vmrss_fmt, "N/A", sizeof(vmrss_fmt));
    }

    printf("pstat for PID=%s (%s)\n", pid, comm);
    printf("----------------------------\n");
    printf("PPid: %ld\n", (ppid_status != -1 ? ppid_status : ppid_stat));
    printf("Threads: %ld\n", (threads_status != -1 ? threads_status : num_threads_stat));
    printf("State (stat char): %c\n", state_char);
    printf("State (status): %s\n", state_status);
    printf("\nCPU times (ticks): utime=%" PRIu64 ", stime=%" PRIu64 "\n", (uint64_t)utime, (uint64_t)stime);
    printf("HZ = %ld ticks/sec\n", hz);
    printf("CPU time total: ticks=%" PRIu64 " => %.6f sec\n", (uint64_t)total_ticks, cpu_seconds);
    printf("\nContext switches: voluntary=%llu, nonvoluntary=%llu\n", voluntary_ctxt, nonvoluntary_ctxt);
    printf("\nMemory: VmRSS: %ld kB (%s)\n", vmrss_kb, vmrss_fmt);
    if (rss_anon_kb >= 0) printf("RssAnon: %ld kB\n", rss_anon_kb);
    else printf("RssAnon: N/A\n");
    if (rss_file_kb >= 0) printf("RssFile: %ld kB\n", rss_file_kb);
    else printf("RssFile: N/A\n");
    printf("\nIO: read_bytes=%llu (%s), write_bytes=%llu (%s)\n",
           read_bytes, read_fmt, write_bytes, write_fmt);

    return 0;
}
