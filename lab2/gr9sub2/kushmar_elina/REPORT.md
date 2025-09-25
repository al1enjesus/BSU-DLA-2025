# Отчет по лабораторной работе 2
Кушмар Элина 9 группа

## Выполненные задания

### Задание A: Мини-супервизор с воркерами

#### Цель
Реализовать систему управления процессами с graceful shutdown, reload и переключением режимов работы.

---

#### Шаги выполнения

1. **Запуск супервизора:**

```bash
python3 supervisor.py --config config.json
```

2. **Проверка запущенных процессов:**

```bash
ps aux | grep supervisor.py
ps aux | grep worker.py | grep -v grep
```

**Вывод:**

- Супервизор: `PID 21344`  
- Воркеры: `PID 21346, 21347, 21348` (3 процесса)  
- Все процессы работают в heavy режиме (`%CPU ~87%`)

---

3. **Тестирование сигналов управления:**

- **SIGUSR1 / SIGUSR2 — переключение режимов:**

```bash
kill -USR1 21344  # Легкий режим
kill -USR2 21344  # Тяжелый режим
```

- **SIGHUP — graceful reload:**

```bash
kill -HUP 21344
```

**Результат:**  
Супервизор перезапустил воркеров с новыми PID (`21682, 21684, 21685, 21686, 21687`).

- **Тест автоперезапуска:**

```bash
kill -KILL 21683  # Убиваем воркера
sleep 3
ps aux | grep worker.py | grep -v grep
```

**Результат:**  
Появился новый воркер `PID 21704` — автоперезапуск работает.

- **SIGTERM — graceful shutdown:**

```bash
kill -TERM 21344
```

---

#### Результаты тестирования сигналов

**Логи graceful shutdown:**

```
SUPERVISOR[21344] 23:38:46 - INFO - Received signal 15, initiating graceful shutdown
SUPERVISOR[21344] 23:38:46 - INFO - Starting graceful shutdown
SUPERVISOR[21344] 23:38:46 - INFO - Sent signal 15 to worker 21682
WORKER[21682] 23:38:46 - INFO - Received signal 15, initiating graceful shutdown
...
WORKER[21686] 23:38:46 - INFO - Final stats: 5741 iterations, total work: 51912.15ms, total sleep: 7433.75ms
WORKER[21686] 23:38:46 - INFO - Worker shutdown complete
```

---

#### Выводы по заданию A

- Супервизор порождает и управляет N воркерами  
- Корректная обработка `SIGTERM` / `SIGINT` / `SIGHUP` / `SIGUSR1` / `SIGUSR2`  
- Graceful shutdown ≤ 5 секунд  
- Graceful reload с перезапуском воркеров  
- Автоперезапуск упавших воркеров с ограничением частоты  
- Корректный "подбор" зомби-процессов  
