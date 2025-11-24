### Установка пакетов

```bash
sudo apt update && sudo apt install -y \
  build-essential gcc gdb strace ltrace \
  linux-tools-common linux-tools-generic perf
```

### Структура проекта
```
lab4/
  gr<группа>sub<подгруппа>/
    ФАМИЛИЯ_ИМЯ/
      REPORT.MD
      README.md         
      src
        syscall_spy.c   
        benchmark.c     
        benchmark_open.c
      logs/             
```
-----

### Компиляция Задания A (`LD_PRELOAD`)


```bash
gcc -shared -fPIC -o libsyscall_spy.so syscall_spy.c -ldl
```

### Компиляция Задания B (`Benchmark`)

```bash
gcc -Wall -Wextra -O2 benchmark.c -o benchmark -lrt

gcc -Wall -Wextra -O2 benchmark_open.c -o benchmark_open -lrt
```

-----

## Воспроизведение экспериментов
### Задание A: Перехват (gcc, make, as)

```bash
# Создание тестовых файлов
echo 'int main() { return 0; }' > test.c
echo 'all:' > test_makefile
echo '	gcc -c test.c -o test.o' >> test_makefile
cat > test.s << 'EOF'
.global _start
_start:
    mov $60, %rax
    mov $0, %rdi
    syscall
EOF

# Эксперимент 1: gcc (перенаправляем вывод в лог)
echo "=== Запуск gcc ==="
LD_PRELOAD=./libsyscall_spy.so gcc test.c -o test 2>&1 | tee ../logs/gcc_output.txt

# Эксперимент 2: make
echo "=== Запуск make ==="
LD_PRELOAD=./libsyscall_spy.so make -f test_makefile 2>&1 | tee ../logs/make_output.txt

# Эксперимент 3: as
echo "=== Запуск as ==="
LD_PRELOAD=./libsyscall_spy.so as test.s -o test.o 2>&1 | tee ../logs/as_output.txt

# Эксперимент: Сравнение статической и динамической программ
echo 'int main() { return 0; }' > static_test.c
gcc -o dynamic_test static_test.c
gcc -static -o static_test static_test.c

echo "--- Динамическая версия (LD_PRELOAD работает) ---"
LD_PRELOAD=./libsyscall_spy.so ./dynamic_test 2>&1 | tee ../logs/dynamic_output.txt

echo "--- Статическая версия (LD_PRELOAD НЕ работает) ---"
LD_PRELOAD=./libsyscall_spy.so ./static_test 2>&1 | tee ../logs/static_output.txt
```

### Задание B: Бенчмарк системных вызовов

Выполните замер времени и циклов CPU, а затем проведите эксперимент со сбросом кэша.

```bash
# Основной запуск (замер времени и циклов)
echo "=== Запуск Benchmark ==="
./benchmark | tee ../logs/benchmark_output.txt

# Анализ через perf stat (Если perf установлен и работает)
# Если perf не установлен, этот шаг выдаст warning
echo "=== Запуск perf stat ==="
perf stat -e cycles,instructions,context-switches,page-faults ./benchmark 2> ../logs/perf_stat_output.txt

# Эксперимент с влиянием кэша страниц (требует root)
echo "=== Эксперимент с кэшем ==="

# Сброс кэша (Холодный кэш)
echo "--- Сброс кэша (Холодный замер) ---"
sudo sync; echo 3 | sudo tee /proc/sys/vm/drop_caches
./benchmark_open | tee ../logs/cache_output.txt

# Горячий кэш
echo "--- Горячий замер ---"
./benchmark_open | tee -a ../logs/cache_output.txt
```

-----

## Очистка

```bash
cd src/
rm -f libsyscall_spy.so benchmark benchmark_open
rm -f test test.c test.o test.s test_makefile dynamic_test static_test static_test.c
rm -f ../logs/*.txt
```