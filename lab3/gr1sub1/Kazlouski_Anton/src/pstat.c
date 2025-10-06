#include <stdio.h>
#include <stdlib.h>
#include <libproc.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <string.h>

// Конвертация байтов в МиБ
double to_mib(uint64_t bytes) {
    return bytes / (1024.0 * 1024.0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <pid>\n", argv[0]);
        return 1;
    }

    pid_t pid = atoi(argv[1]);
    struct proc_taskinfo pti;
    struct proc_taskallinfo tai;

    // Основная информация о задаче
    int ret = proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &pti, sizeof(pti));
    if (ret <= 0) {
        fprintf(stderr, "Error: cannot read PROC_PIDTASKINFO for PID %d\n", pid);
        return 1;
    }

    // Информация для PPid
    ret = proc_pidinfo(pid, PROC_PIDTASKALLINFO, 0, &tai, sizeof(tai));
    if (ret <= 0) {
        fprintf(stderr, "Warning: cannot read PROC_PIDTASKALLINFO\n");
    }

    printf("=== pstat summary for PID %d ===\n", pid);
    printf("Command: %s\n", tai.pbsd.pbi_comm);
    printf("PPid: %d\n", tai.pbsd.pbi_ppid);
    printf("Threads: %d\n", pti.pti_threadnum);
    printf("State: %d (see 'ps -o state -p %d')\n", tai.pbsd.pbi_status, pid);

    double utime = pti.pti_total_user / 1e6;
    double stime = pti.pti_total_system / 1e6;
    double total_cpu = utime + stime;

    printf("User time: %.3f s\n", utime);
    printf("System time: %.3f s\n", stime);
    printf("Total CPU time: %.3f s\n", total_cpu);

    printf("VmRSS: %.2f MiB\n", to_mib(pti.pti_resident_size));
    printf("VmSize: %.2f MiB\n", to_mib(pti.pti_virtual_size));

    // IO-метрики в macOS нужно получать отдельно через fs_usage или proc_pid_rusage
    printf("Read bytes: n/a (use fs_usage or proc_pid_rusage)\n");
    printf("Write bytes: n/a (use fs_usage or proc_pid_rusage)\n");

    printf("Voluntary ctxt switches: %llu\n", (unsigned long long)pti.pti_csw);
    printf("Involuntary ctxt switches: n/a\n"); // точное значение недоступно

    printf("\nHint: use 'vmmap -summary %d' to inspect RssAnon/RssFile.\n", pid);
    return 0;
}
