#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdbool.h>
#include <ctype.h>
#include <libproc.h>
#include <mach/mach.h>

#define TARGET_PROCESS "Telegram"
#define CHECK_INTERVAL_SEC 1
#define NOTIFY_TITLE "⚠️ Внимание!"
#define NOTIFY_MSG "Запуск " TARGET_PROCESS " запрещён!"

static volatile sig_atomic_t running = 1;

void signal_handler(int sig) {
    running = 0;
}

void show_notification(void) {
    pid_t pid = fork();
    if (pid == 0) {
        char script[512];
        snprintf(script, sizeof(script),
                 "osascript -e 'display notification \"%s\" with title \"%s\"'",
                 NOTIFY_MSG, NOTIFY_TITLE);
        system(script);
        _exit(0);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    }
}

int get_process_list(pid_t *pids, int max_pids) {
    int count = proc_listpids(PROC_ALL_PIDS, 0, pids, max_pids * sizeof(pid_t));
    return count / sizeof(pid_t);
}

bool get_process_name(pid_t pid, char *buffer, size_t buffer_size) {
    memset(buffer, 0, buffer_size);
    int ret = proc_name(pid, buffer, buffer_size);
    return ret > 0;
}

bool get_process_path(pid_t pid, char *buffer, size_t buffer_size) {
    memset(buffer, 0, buffer_size);
    int ret = proc_pidpath(pid, buffer, buffer_size);
    return ret > 0;
}

bool is_target_process(pid_t pid) {
    char process_name[256];
    char process_path[PROC_PIDPATHINFO_MAXSIZE];

    if (!get_process_name(pid, process_name, sizeof(process_name))) {
        return false;
    }

    if (!get_process_path(pid, process_path, sizeof(process_path))) {
        return false;
    }

    if (strcasestr(process_name, "telegram") != NULL ||
        strcasestr(process_name, "Telegram") != NULL) {
        return true;
    }

    if (strcasestr(process_path, "Telegram") != NULL ||
        strcasestr(process_path, "telegram") != NULL) {
        return true;
    }

    char command[512];
    snprintf(command, sizeof(command),
             "ps -p %d -o command | grep -i telegram", pid);

    FILE *fp = popen(command, "r");
    if (fp) {
        char result[256];
        if (fgets(result, sizeof(result), fp) != NULL) {
            if (strcasestr(result, "telegram") != NULL) {
                pclose(fp);
                return true;
            }
        }
        pclose(fp);
    }

    return false;
}

void terminate_process(pid_t pid) {
    printf("[!] Обнаружен Telegram (PID: %d)\n", (int)pid);

    char process_name[256];
    char process_path[PROC_PIDPATHINFO_MAXSIZE];

    if (get_process_name(pid, process_name, sizeof(process_name))) {
        printf("[!] Имя процесса: %s\n", process_name);
    }

    if (get_process_path(pid, process_path, sizeof(process_path))) {
        printf("[!] Путь: %s\n", process_path);
    }

    show_notification();

    if (kill(pid, SIGTERM) == 0) {
        printf("[!] Отправлен SIGTERM процессу %d\n", (int)pid);

        sleep(2);

        if (kill(pid, 0) == 0) {
            printf("[!] Принудительное завершение (SIGKILL)\n");
            kill(pid, SIGKILL);
        }
    } else {
        printf("[!] Не удалось отправить SIGTERM: %s\n", strerror(errno));
    }

    char script[512];
    snprintf(script, sizeof(script),
             "osascript -e 'tell application \"Telegram\" to quit' 2>/dev/null");
    system(script);

    snprintf(script, sizeof(script),
             "osascript -e 'tell application \"Telegram Desktop\" to quit' 2>/dev/null");
    system(script);

    snprintf(script, sizeof(script),
             "osascript -e 'tell application \"System Events\" to tell process \"Telegram\" to click menu item \"Выход\" of menu \"Telegram\" of menu bar 1' 2>/dev/null");
    system(script);
}

void monitor_loop(void) {
    printf("[*] Запуск Telegram Watchdog для macOS...\n");
    printf("[*] Мониторинг процесса: %s\n", TARGET_PROCESS);
    printf("[*] Интервал проверки: %d секунд\n", CHECK_INTERVAL_SEC);
    printf("[*] Нажмите Ctrl+C для остановки\n");
    printf("[*] PID текущего процесса: %d\n\n", getpid());

    uid_t current_uid = getuid();
    printf("[*] Текущий UID: %d\n", current_uid);

    while (running) {
        pid_t pids[2048];
        int count = get_process_list(pids, sizeof(pids)/sizeof(pids[0]));

        if (count <= 0) {
            sleep(CHECK_INTERVAL_SEC);
            continue;
        }

        for (int i = 0; i < count && running; i++) {
            pid_t pid = pids[i];

            if (pid <= 0) continue;

            if (pid == getpid()) continue;

            struct proc_bsdinfo proc_info;
            if (proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &proc_info, sizeof(proc_info)) > 0) {
                if (proc_info.pbi_uid != current_uid) {
                    continue;
                }
            }

            if (is_target_process(pid)) {
                terminate_process(pid);

                sleep(1);
                break;
            }
        }

        sleep(CHECK_INTERVAL_SEC);
    }
}

int main(void) {
    if (getuid() != 0) {
        printf("[!] Внимание: Программа запущена без root прав\n");
        printf("[!] Могут быть проблемы с доступом к процессам\n");
        printf("[!] Для полного доступа запустите с sudo\n");
        printf("[*] Продолжить? (y/n): ");

        char answer;
        scanf("%c", &answer);
        if (answer != 'y' && answer != 'Y') {
            printf("[*] Выход...\n");
            return 0;
        }
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);

    monitor_loop();

    printf("\n[+] Остановка watchdog...\n");
    return 0;
}