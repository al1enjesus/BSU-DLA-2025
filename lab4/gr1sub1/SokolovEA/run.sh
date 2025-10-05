#!/bin/bash

echo "============================================="
echo "Лабораторная 4 - Системные вызовы"
echo "Группа программ: 11 % 4 = 3 → gcc, make, as"
echo "============================================="

cd src/

echo -e "\n1. Сборка всех программ..."
make all

echo -e "\n2. Задание A: LD_PRELOAD на gcc (компиляция)"
echo "Команда: LD_PRELOAD=./libsyscall_spy.so gcc -c hello_static.c -o test.o"
echo "Результат (первые 15 строк):"
LD_PRELOAD=./libsyscall_spy.so gcc -c hello_static.c -o test.o 2>&1 | head -15

echo -e "\n3. Задание A: Проверка на статической программе"
echo "Команда: LD_PRELOAD=./libsyscall_spy.so ./hello_static"
echo "Результат (LD_PRELOAD не работает на статических программах):"
LD_PRELOAD=./libsyscall_spy.so ./hello_static

echo -e "\n4. Задание B: Бенчмарк системных вызовов"
echo "Команда: ./benchmark"
echo "Результат:"
./benchmark

echo -e "\n5. Задание C*: Упрощённый strace"
echo "Команда: ./mytracer /bin/true"
echo "Результат (первые 10 строк):"
timeout 5s ./mytracer /bin/true | head -10

echo -e "\n6. Измерение overhead трассировки"
echo "Без трассировки:"
time /bin/true

echo "С трассировкой:"
time ./mytracer /bin/true > /dev/null 2>&1

echo -e "\n============================================="
echo "Лабораторная работа выполнена!"
echo "Полный отчёт в файле REPORT.MD"
echo "============================================="