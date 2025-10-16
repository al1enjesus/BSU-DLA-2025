#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <string.h>
#include <sys/wait.h>

#define ITERATIONS 100000

// Функция, которая создает много системных вызовов (низкий IPC)
void systemcall_intensive_workload() {
    for (int i = 0; i < ITERATIONS; i++) {
        // Много быстрых системных вызовов - низкий IPC
        getpid();
        getppid();
        getuid();
        getgid();

        // Иногда создаем дочерний процесс для context-switches
        if (i % 5000 == 0) {
            pid_t pid = fork();
            if (pid == 0) {
                // Дочерний процесс делает немного работы и завершается
                for (int j = 0; j < 100; j++) {
                    getpid();
                }
                exit(0);
            }
            else if (pid > 0) {
                wait(NULL); // Ждем завершения дочернего процесса
            }
        }
    }
}

// Функция с файловыми операциями для page-faults
void file_operations_workload() {
    for (int i = 0; i < ITERATIONS / 10; i++) {
        char filename[64];
        snprintf(filename, sizeof(filename), "/tmp/bench_open_%d", i % 20);

        // Открытие, запись, закрытие - создает page-faults
        int fd = open(filename, O_RDWR | O_CREAT, 0644);
        if (fd != -1) {
            // Пишем данные
            char data[512];
            memset(data, i % 256, sizeof(data));
            write(fd, data, sizeof(data));

            // Закрываем файл
            close(fd);
        }

        // Иногда удаляем файл
        if (i % 100 == 0) {
            unlink(filename);
        }
    }
}

// Функция с минимальными вычислениями в userspace
void minimal_computation() {
    volatile int counter = 0;
    for (int i = 0; i < ITERATIONS / 5; i++) {
        // Минимальные вычисления - не увеличиваем IPC сильно
        counter = (counter + i) % 1000;
    }
}

int main() {
    printf("Starting benchmark_open with %d iterations...\n", ITERATIONS);

    struct timeval start, end;
    gettimeofday(&start, NULL);

    // Запускаем workloads которые дадут нужные метрики
    systemcall_intensive_workload();  // Низкий IPC, context-switches
    file_operations_workload();       // Page-faults  
    minimal_computation();            // Минимальные вычисления

    gettimeofday(&end, NULL);

    double elapsed = (end.tv_sec - start.tv_sec) +
        (end.tv_usec - start.tv_usec) * 1e-6;

    printf("Benchmark completed in %.9f seconds\n", elapsed);

    // Очистка временных файлов
    for (int i = 0; i < 20; i++) {
        char filename[64];
        snprintf(filename, sizeof(filename), "/tmp/bench_open_%d", i);
        unlink(filename);
    }

    return 0;
}