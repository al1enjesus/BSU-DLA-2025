/* mem_touch.c
   Простой инструмент для постепенного увеличения RSS и наблюдения OOM/лимитов.
   Компиляция:
     gcc -O2 mem_touch.c -o mem_touch
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <inttypes.h>

static volatile sig_atomic_t stop_flag = 0;
static volatile sig_atomic_t inc_flag = 0;
static volatile sig_atomic_t dec_flag = 0;

void on_sigterm(int s){ (void)s; stop_flag = 1; }
void on_sigusr1(int s){ (void)s; inc_flag = 1; }
void on_sigusr2(int s){ (void)s; dec_flag = 1; }

static void print_status(const char *tag) {
    // вывести VmSize и VmRSS из /proc/self/status (пара первых строк)
    FILE *f = fopen("/proc/self/status", "r");
    if (!f) { perror("fopen /proc/self/status"); return; }
    char line[256];
    printf("=== %s: /proc/self/status ===\n", tag);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmSize:", 7) == 0 || strncmp(line, "VmRSS:", 6) == 0 || strncmp(line, "Uid:", 4) == 0) {
            fputs(line, stdout);
        }
    }
    fclose(f);
    fflush(stdout);
}

int main(int argc, char **argv){
    size_t target_mb = 512;
    size_t step_mb = 64;
    int sleep_ms = 200;
    size_t max_steps = 1000;
    size_t limit_as_mb = 0; // 0 — не ставим
    int opt;

    // простая парсинг аргументов
    for (int i=1;i<argc;i++){
        if (strcmp(argv[i],"--rss-mb")==0 && i+1<argc){ target_mb = (size_t)atoi(argv[++i]); }
        else if (strcmp(argv[i],"--step-mb")==0 && i+1<argc){ step_mb = (size_t)atoi(argv[++i]); }
        else if (strcmp(argv[i],"--sleep-ms")==0 && i+1<argc){ sleep_ms = atoi(argv[++i]); }
        else if (strcmp(argv[i],"--limit-as-mb")==0 && i+1<argc){ limit_as_mb = (size_t)atoi(argv[++i]); }
        else if (strcmp(argv[i],"--max-steps")==0 && i+1<argc){ max_steps = (size_t)atoi(argv[++i]); }
        else if (strcmp(argv[i],"--help")==0){ printf("Usage: %s [--rss-mb N] [--step-mb M] [--sleep-ms ms] [--limit-as-mb L]\n", argv[0]); return 0; }
    }

    // сигналы
    signal(SIGTERM, on_sigterm);
    signal(SIGINT, on_sigterm);
    signal(SIGUSR1, on_sigusr1);
    signal(SIGUSR2, on_sigusr2);

    // установить RLIMIT_AS если требуется
    if (limit_as_mb > 0) {
        struct rlimit rl;
        rl.rlim_cur = rl.rlim_max = (rlim_t)limit_as_mb * 1024 * 1024;
        if (setrlimit(RLIMIT_AS, &rl) != 0) {
            perror("setrlimit(RLIMIT_AS)");
            // продолжаем, но сообщим
        } else {
            printf("set RLIMIT_AS = %zu MB\n", limit_as_mb);
        }
    }

    // динамический список блоков
    size_t steps_allocated = 0;
    char **blocks = calloc(max_steps+4, sizeof(char*));
    if (!blocks) { perror("calloc"); return 2; }

    size_t target_steps = (target_mb + step_mb - 1) / step_mb;
    printf("Starting mem_touch: target_mb=%zu step_mb=%zu sleep_ms=%d (target_steps=%zu)\n",
           target_mb, step_mb, sleep_ms, target_steps);

    print_status("start");

    for (size_t iter = 0; !stop_flag && iter < 1000000; ++iter) {
        // handle user signals
        if (inc_flag) { target_steps += 1; inc_flag = 0; printf("SIGUSR1: increasing target_steps -> %zu\n", target_steps); }
        if (dec_flag && target_steps > 0) { target_steps -= 1; dec_flag = 0; printf("SIGUSR2: decreasing target_steps -> %zu\n", target_steps); }

        // allocate until we reach target
        while (steps_allocated < target_steps) {
            if (steps_allocated >= max_steps) { fprintf(stderr, "reached max_steps limit (%zu)\n", max_steps); break; }
            size_t bytes = step_mb * 1024UL * 1024UL;
            char *p = malloc(bytes);
            if (!p) {
                perror("malloc");
                print_status("malloc-failed");
                // при неудаче — завершаем с кодом 3 (показывает OOM/limit)
                fprintf(stderr, "Allocation failed at step %zu (requested %zu MB total)\n", steps_allocated, (steps_allocated*step_mb));
                free(blocks);
                return 3;
            }
            // touch memory чтобы гарантировать RSS увеличился
            for (size_t i = 0; i < bytes; i += 4096) p[i] = 0xA5;
            blocks[steps_allocated++] = p;
            printf("[mem_touch] allocated step %zu -> total_alloc_steps=%zu (%zu MB)\n",
                   steps_allocated, steps_allocated, steps_allocated * step_mb);
            print_status("after-alloc");
            // небольшой небольшй паузу — чтобы можно было снимать pidstat между шагами
            usleep(1000*50);
        }

        // если переполнено — ждем
        // sleep для наблюдения
        if (sleep_ms > 0) usleep(sleep_ms * 1000);

        // если нужно уменьшить цель — освобождаем последние блоки
        while (steps_allocated > target_steps) {
            char *p = blocks[--steps_allocated];
            if (p) { free(p); blocks[steps_allocated] = NULL; printf("[mem_touch] freed one step -> steps=%zu\n", steps_allocated); print_status("after-free"); }
        }

        // периодический вывод
        if (iter % 5 == 0) print_status("loop");

        // cooperative exit check
        if (stop_flag) break;
    }

    printf("mem_touch exiting; freeing %zu blocks\n", steps_allocated);
    for (size_t i = 0; i < steps_allocated; ++i) if (blocks[i]) free(blocks[i]);
    free(blocks);
    print_status("end");
    return 0;
}

