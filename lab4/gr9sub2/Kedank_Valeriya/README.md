## Запуск экспериментов. Задание А.

## Автоматический запуск всех тестов
```bash
./run.sh
```

## Компиляция библиотеки

```bash
cd src
gcc -shared -fPIC -o libsyscall_spy.so syscall_spy.c -ldl
```

## Ручное тестирование

``` bash
# Простой тест
LD_PRELOAD=./src/libsyscall_spy.so cat test.txt

# Тестирование на программах
LD_PRELOAD=./src/libsyscall_spy.so gcc -c simple.c -o simple.o
```

#Задание B: Benchmark

cd src

## Компиляция бенчмарка
gcc -o benchmark benchmark.c

## Запуск бенчмарка
./benchmark

## Запуск с perf (требует sudo)
sudo perf stat -e cycles,instructions,context-switches,page-faults ./benchmark
