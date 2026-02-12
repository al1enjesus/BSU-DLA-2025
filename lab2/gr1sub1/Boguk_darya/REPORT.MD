# Мини-супервизор с воркерами
Цель
Реализовать процесс-родителя, который порождает дочерние процессы, имитирующие работу, поддерживает сигналы для работы с процессами 
Шаги 
Созданы файлы для супервизора, воркера, файл для их сборки
Супервизор:
Первая функция используется для создания и настройки дочерних процессов. fork() создает копию текущего процесса и возвращает pid(0-если ребенок, другой - если родитель), если ребенок, то загрудается код из воркера и начинается его выполнение, в противном случае завершает команду с кодом 1, далее используем setpriority() для дочернего процесса, делаем структуру для хранения маски CPU,зануляем ее и потом устанавливаем в зависимости от индекса бит в 1, то есть для четных индексов назначаем ядро 0, для нечетных 1, далее системный вызов sched_setaffinity привязывает процесса к конкретному CPU
Дальше функция для обработки завершения процессов, которая ждет любого дочернего процесса и смотри завершился ли он, потом идут обработчики флагов, реагирующие на входящие сигналы
SIGTERM/SIGINT: корректное завершение — послать воркерам «стоп» и дождаться их выхода graceful shutdown.
SIGHUP: «graceful reload» — перечитать конфиг и «мягко» перезапустить воркеров без потери работы (допускается короткая пауза).
SIGUSR1: переключить воркеров в «лёгкий» режим нагрузки, SIGUSR2 — обратно в «тяжёлый».
Далее идет создание воркеров, обработка перезагрузки и сигналов
Воркер:
задаем необходимые переменные, далее функция иммитации работы, цикл, где в зависимости от режима назначается работа и находится счетчик, который считает количество раз, сколько послали воркер работать
Команды и результаты их выполнения
# kill - SIGHUP <pid worker> - перезапуск всех воркеров
```
[Worker 7651] tick=6672 mode=heavy CPU=1
[Worker 7652] tick=4281 mode=heavy CPU=0
[Worker 7651] tick=6673 mode=heavy CPU=1
[Worker 7650] tick=4337 mode=heavy CPU=0
[Worker 7651] tick=6674 mode=heavy CPU=1
[Supervisor] reload requested
[Worker 7652] tick=4282 mode=heavy CPU=0
[Worker 7650] tick=4338 mode=heavy CPU=0
[Worker 7650] exiting gracefully
[Supervisor] started worker 0 (pid=7920)
[Worker 7651] tick=6675 mode=heavy CPU=1
[Worker 7651] exiting gracefully
[Supervisor] started worker 1 (pid=7921)
[Worker 7652] tick=4283 mode=heavy CPU=0
[Worker 7652] exiting gracefully
[Supervisor] started worker 2 (pid=7922)
[Worker 7920] tick=1 mode=heavy CPU=0
[Worker 7921] tick=1 mode=heavy CPU=1
[Worker 7920] tick=2 mode=heavy CPU=0
[Worker 7921] tick=2 mode=heavy CPU=1
```
# kill -SIGTERM <pid supervisor> - остановка всех воркеров без перезапуска
```
[Worker 7921] tick=37 mode=heavy CPU=1
[Supervisor] shutting down
[Worker 7920] tick=22 mode=heavy CPU=0
[Worker 7920] exiting gracefully
[Worker 7922] tick=22 mode=heavy CPU=0
[Worker 7922] exiting gracefully
[Worker 7921] tick=38 mode=heavy CPU=1
[Worker 7921] exiting gracefully
```
# kill -9 <pid worker> - остановит одного воркера, но код это отследит и перезапустит его
```
[Worker 8648] tick=3205 mode=heavy CPU=0
[Worker 8646] tick=7773 mode=heavy CPU=0
[Worker 8646] tick=7776 mode=heavy CPU=0
[Worker 8672] tick=3500 mode=heavy CPU=1
[Supervisor] worker 2 (pid=8648) exited, restarting...
[Supervisor] started worker 2 (pid=8743)
[Worker 8646] tick=7777 mode=heavy CPU=0
[Worker 8646] tick=7783 mode=heavy CPU=0
[Worker 8672] tick=3505 mode=heavy CPU=1
[Worker 8743] tick=1 mode=heavy CPU=0
```
# kill -USR1/-USR2 <pid worker> - меняет режимы работы воркеров
```
[Worker 8329] tick=5872 mode=light CPU=0
[Worker 8327] tick=8021 mode=light CPU=0
[Worker 8328] tick=7145 mode=light CPU=1
[Worker 8327] tick=8022 mode=heavy CPU=0
[Worker 8329] tick=5873 mode=heavy CPU=0
[Worker 8327] tick=8023 mode=heavy CPU=0
```
```
[Worker 8438] tick=2803 mode=heavy CPU=1
[Worker 8437] tick=2862 mode=heavy CPU=0
[Worker 8439] tick=1068 mode=heavy CPU=0
[Worker 8438] tick=2804 mode=heavy CPU=1
[Worker 8438] tick=2805 mode=light CPU=1
[Worker 8437] tick=2863 mode=light CPU=0
[Worker 8439] tick=1069 mode=light CPU=0
[Worker 8438] tick=2806 mode=light CPU=1
```
Выводы
Все команды отработали корректно и ожидаемо

# Планирование
Цель
Снять метрики, посмотреть, как nice, affinity влияют на время ожидания и cpu

Шаги
Посмотрим для каждого воркера pid, nice
```
darya@darya-IdeaPad-3-15ALC6:~$ ps -o pid,ni,comm -p 9437
    PID  NI COMMAND
   9437   0 worker
darya@darya-IdeaPad-3-15ALC6:~$ ps -o pid,ni,comm -p 9438
    PID  NI COMMAND
   9438  10 worker
darya@darya-IdeaPad-3-15ALC6:~$ ps -o pid,ni,comm -p 9439
    PID  NI COMMAND
   9439  10 worker
```
Получили 2 воркера с nice=10, то есть они будут получать время cpu, когда воркер 9437 будет отдыхать

Для каждого воркера посмотрим, на каком процессоре он работает
```
darya@darya-IdeaPad-3-15ALC6:~$ taskset -pc 9437
pid 9437's current affinity list: 0
darya@darya-IdeaPad-3-15ALC6:~$ taskset -pc 9438
pid 9438's current affinity list: 1
darya@darya-IdeaPad-3-15ALC6:~$ taskset -pc 9439
pid 9439's current affinity list: 0
```
видим, что они распределены, как мы их назначали, в зависимости от индекса создания

Посмотри характеристики каждого воркера
```
darya@darya-IdeaPad-3-15ALC6:~$ pidstat -u 1 10 -p 9437
Linux 5.15.0-139-generic (darya-IdeaPad-3-15ALC6)  22.09.2025  _x86_64_(12 CPU)

15:41:14      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
15:41:15     1000      9437    4,00   55,00    0,00   34,00   59,00     0  worker
15:41:16     1000      9437    5,00   54,00    0,00   34,00   59,00     0  worker
15:41:17     1000      9437    5,00   53,00    0,00   34,00   58,00     0  worker
15:41:18     1000      9437    3,00   44,00    0,00   45,00   47,00     0  worker
15:41:19     1000      9437    4,00   52,00    0,00   36,00   56,00     0  worker
15:41:20     1000      9437    3,00   49,00    0,00   39,00   52,00     0  worker
15:41:21     1000      9437    5,00   54,00    0,00   33,00   59,00     0  worker
15:41:22     1000      9437    4,00   55,00    0,00   33,00   59,00     0  worker
15:41:23     1000      9437    3,00   54,00    0,00   36,00   57,00     0  worker
15:41:24     1000      9437    5,00   55,00    0,00   33,00   60,00     0  worker
Average:     1000      9437    4,10   52,50    0,00   35,70   56,60     -  worker
```

```darya@darya-IdeaPad-3-15ALC6:~$ pidstat -u 1 10 -p 9438
Linux 5.15.0-139-generic (darya-IdeaPad-3-15ALC6)  22.09.2025  _x86_64_ (12 CPU)

15:41:49      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
15:41:50     1000      9438    3,00   42,00    0,00   48,00   45,00     1  worker
15:41:51     1000      9438    2,00   44,00    0,00   47,00   46,00     1  worker
15:41:52     1000      9438    6,00   40,00    0,00   47,00   46,00     1  worker
15:41:53     1000      9438    4,00   40,00    0,00   48,00   44,00     1  worker
15:41:54     1000      9438    3,00   43,00    0,00   47,00   46,00     1  worker
15:41:55     1000      9438    4,00   41,00    0,00   47,00   45,00     1  worker
15:41:56     1000      9438    3,00   43,00    0,00   48,00   46,00     1  worker
15:41:57     1000      9438    3,00   42,00    0,00   47,00   45,00     1  worker
15:41:58     1000      9438    3,00   43,00    0,00   47,00   46,00     1  worker
15:41:59     1000      9438    3,00   42,00    0,00   48,00   45,00     1  worker
Average:     1000      9438    3,40   42,00    0,00   47,40   45,40     -  worker
```

```
darya@darya-IdeaPad-3-15ALC6:~$ pidstat -u 1 10 -p 9439
Linux 5.15.0-139-generic (darya-IdeaPad-3-15ALC6)  22.09.2025  _x86_64_ (12 CPU)

15:42:13      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
15:42:14     1000      9439    1,00   12,00    0,00   87,00   13,00     0  worker
15:42:15     1000      9439    0,00   12,00    0,00   86,00   12,00     0  worker
15:42:16     1000      9439    1,00   12,00    0,00   82,00   13,00     0  worker
15:42:17     1000      9439    3,00   11,00    0,00   84,00   14,00     0  worker
15:42:18     1000      9439    1,00   10,00    0,00   83,00   11,00     0  worker
15:42:19     1000      9439    1,00   11,00    0,00   83,00   12,00     0  worker
15:42:20     1000      9439    2,00    9,00    0,00   84,00   11,00     0  worker
15:42:21     1000      9439    0,00   10,00    0,00   85,00   10,00     0  worker
15:42:22     1000      9439    2,00   11,00    0,00   82,00   13,00     0  worker
15:42:23     1000      9439    0,00   10,00    0,00   85,00   10,00     0  worker
Average:     1000      9439    1,10   10,80    0,00   84,10   11,90     -  worker
```

Выводы
Воркер 9437 имеет больший приоритет и загружен поэтому больше, проблемы с планирование, потому что воркер 9439 имеет большой wait 

Попробуем уравнять приоритетность воркеров на одном ядре, получим
```
darya@darya-IdeaPad-3-15ALC6:~$ renice +10 -p 9437
9437 (process ID) old priority 0, new priority 10
darya@darya-IdeaPad-3-15ALC6:~$ pidstat -u 1 10 -p 9439
Linux 5.15.0-139-generic (darya-IdeaPad-3-15ALC6)  22.09.2025  _x86_64_ (12 CPU)

15:50:41      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
15:50:42     1000      9439    2,00   23,00    0,00   71,00   25,00     0  worker
15:50:43     1000      9439    3,00   24,00    0,00   69,00   27,00     0  worker
15:50:44     1000      9439    1,00   23,00    0,00   70,00   24,00     0  worker
15:50:45     1000      9439    2,00   23,00    0,00   72,00   25,00     0  worker
15:50:46     1000      9439    2,00   23,00    0,00   69,00   25,00     0  worker
15:50:47     1000      9439    3,00   22,00    0,00   72,00   25,00     0  worker
15:50:48     1000      9439    2,00   23,00    0,00   70,00   25,00     0  worker
15:50:49     1000      9439    2,00   22,00    0,00   71,00   24,00     0  worker
15:50:50     1000      9439    3,00   23,00    0,00   69,00   26,00     0  worker
15:50:51     1000      9439    2,00   23,00    0,00   71,00   25,00     0  worker
Average:     1000      9439    2,20   22,90    0,00   70,40   25,10     -  worker
darya@darya-IdeaPad-3-15ALC6:~$ pidstat -u 1 10 -p 9437
Linux 5.15.0-139-generic (darya-IdeaPad-3-15ALC6)  22.09.2025  _x86_64_ (12 CPU)

15:50:56      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
15:50:57     1000      9437    1,00   23,00    0,00   71,00   24,00     0  worker
15:50:58     1000      9437    1,00   24,00    0,00   70,00   25,00     0  worker
15:50:59     1000      9437    1,00   24,00    0,00   71,00   25,00     0  worker
15:51:00     1000      9437    1,00   24,00    0,00   70,00   25,00     0  worker
15:51:01     1000      9437    2,00   23,00    0,00   71,00   25,00     0  worker
15:51:02     1000      9437    1,00   23,00    0,00   70,00   24,00     0  worker
15:51:03     1000      9437    1,00   24,00    0,00   70,00   25,00     0  worker
15:51:04     1000      9437    3,00   24,00    0,00   70,00   27,00     0  worker
15:51:05     1000      9437    1,00   24,00    0,00   70,00   25,00     0  worker
15:51:06     1000      9437    2,00   23,00    0,00   69,00   25,00     0  worker
Average:     1000      9437    1,40   23,60    0,00   70,20   25,00     -  worker
```
Выводы
Показатели стали лучше, почти одинаковые, одинаково используют cpu и ждут равное количество времени


# Ответы на вопросы для отчета
1. Процесс - изолированное адресное пространство, свои ресурсы
 поток -разделяет память и ресурсы процесса
2. Влияет на вес процесса в планировщике, чем меньше nice, тем больше cpu времени, пределы от -20 до 19

3.+: предсказуемая производительность, -:неравномерная нагрузка
4. RLIMIT_AS - ограничивает всё виртуальное адресное пространство
RLIMIT_DATA - только сегмент данных (heap)
RLIMIT_RSS - в физической памяти
Почему RLIMIT_RSS игнорируется: неточность, сложность реализации
5. Процесс завершился, но родитель не вызвал wait() для чтения статуса,избежать можно за счет SIGCHL waitpid(WNOHANG)
6.Shutdown: завершить текущие задачи и выход. Reload: новые воркеры запускаются параллельно со старыми, старые завершаются после обработки текущих запросов. Reload безопаснее
7.Он будет считать лимиты контейнера своими лимитами и будет показывать метрики относительно этих лимитов
