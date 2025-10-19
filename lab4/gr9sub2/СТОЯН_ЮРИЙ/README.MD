 Быстрый старт
1. Клонирование и настройка
```bash
# Переход в папку проекта
cd lab4
```
# Компиляция всех программ
```
make all
```
2. Тестирование задания A (LD_PRELOAD)
```bash
# Автоматический тест
make test_a
```
# Ручной тест на программе find
```bash
LD_PRELOAD=./libsyscall_spy.so find /tmp -name "*.txt"
```
# Ручной тест на программе cp
```bash
LD_PRELOAD=./libsyscall_spy.so cp file1.txt file2.txt
```
3. Тестирование задания B (Benchmark)
```bash
# Автоматический тест
make test_b
```
# Запуск основного бенчмарка
```bash
./benchmark
```
# Запуск с холодным кэшем
./benchmark_cold


# Анализ через perf stat
```bash
make perf_report
```
4. Сбор данных для отчета
bash
# Создание тестовых данных и сбор логов
```bash
make report_data
```
Команды Makefile
Команда	Описание
make all	Компиляция всех программ
make test_a	Тест задания A (LD_PRELOAD)
make test_b	Тест задания B (Benchmark)
make test	Полный тест всех заданий
make perf_report	Анализ производительности через perf stat
make report_data	Сбор данных для отчета
make setup_test	Создание тестовой структуры файлов
make clean	Очистка скомпилированных файлов
make help	Справка по доступным командам