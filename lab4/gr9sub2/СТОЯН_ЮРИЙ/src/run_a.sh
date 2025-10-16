#!/bin/bash

echo "=== ПОЛНЫЙ ТЕСТ ДЛЯ ОТЧЕТА LAB 4 ==="
echo "Вариант: find, tar, cp"
echo ""

# Создаем папку для логов
echo "=== СОЗДАНИЕ ПАПКИ ДЛЯ ЛОГОВ ==="
mkdir -p logs
echo "Папка logs создана"

# 1. Компиляция библиотеки
echo "=== 1. КОМПИЛЯЦИЯ БИБЛИОТЕКИ ==="
gcc -shared -fPIC -o libsyscall_spy.so syscall_spy.c -ldl
echo "Библиотека скомпилирована"

echo -e "\n--- Проверка файла ---"
file libsyscall_spy.so > logs/01_file_check.txt
cat logs/01_file_check.txt

echo -e "\n--- Проверка зависимостей ---"
ldd libsyscall_spy.so > logs/02_ldd_check.txt
cat logs/02_ldd_check.txt

# 2. Создание тестовых данных
echo -e "\n=== 2. СОЗДАНИЕ ТЕСТОВЫХ ДАННЫХ ==="
mkdir -p test_data/folder1
mkdir -p test_data/subdir
echo "File 1 content" > test_data/file1.txt
echo "File 2 content" > test_data/file2.txt
echo "File in folder1" > test_data/folder1/file3.txt
echo "File in subdir" > test_data/subdir/file4.txt
echo "Тестовые данные созданы"
tree test_data > logs/03_test_structure.txt
cat logs/03_test_structure.txt

# 3. Сбор полных логов
echo -e "\n=== 3. СБОР ПОЛНЫХ ЛОГОВ ==="

echo -e "\n--- FIND ---"
LD_PRELOAD=./libsyscall_spy.so find test_data -type f > logs/04_find_output.txt 2>&1
echo "Лог find сохранен (logs/04_find_output.txt)"

echo -e "\n--- TAR CREATE ---"
LD_PRELOAD=./libsyscall_spy.so tar -cf archive.tar test_data > logs/05_tar_create_output.txt 2>&1
echo "Лог tar create сохранен (logs/05_tar_create_output.txt)"

echo -e "\n--- TAR EXTRACT ---"
mkdir -p extracted
LD_PRELOAD=./libsyscall_spy.so tar -xf archive.tar -C extracted > logs/06_tar_extract_output.txt 2>&1
echo "Лог tar extract сохранен (logs/06_tar_extract_output.txt)"

echo -e "\n--- CP ---"
LD_PRELOAD=./libsyscall_spy.so cp test_data/file1.txt copied.txt > logs/07_cp_output.txt 2>&1
echo "Лог cp сохранен (logs/07_cp_output.txt)"

# 4. Показываем логи
echo -e "\n=== 4. ПОЛНЫЕ ЛОГИ ПРОГРАММ ==="

echo -e "\n--- FIND (первые 50 строк) ---"
head -50 logs/04_find_output.txt

echo -e "\n--- TAR CREATE (первые 40 строк) ---"
head -40 logs/05_tar_create_output.txt

echo -e "\n--- TAR EXTRACT (первые 30 строк) ---"
head -30 logs/06_tar_extract_output.txt

echo -e "\n--- CP (полный вывод) ---"
cat logs/07_cp_output.txt

# 5. Сравнительная таблица
echo -e "\n=== 5. СРАВНИТЕЛЬНАЯ ТАБЛИЦА ВЫЗОВОВ ==="

# Создаем файл с таблицей
echo "Программа    | OPEN | OPENAT | READ | WRITE | CLOSE" > logs/08_comparison_table.txt
echo "-------------|------|--------|------|-------|------" >> logs/08_comparison_table.txt

echo -n "FIND         | " >> logs/08_comparison_table.txt
grep -c "OPEN:" logs/04_find_output.txt | tr -d '\n' >> logs/08_comparison_table.txt
echo -n " | " >> logs/08_comparison_table.txt
grep -c "OPENAT:" logs/04_find_output.txt | tr -d '\n' >> logs/08_comparison_table.txt
echo -n " | " >> logs/08_comparison_table.txt
grep -c "READ:" logs/04_find_output.txt | tr -d '\n' >> logs/08_comparison_table.txt
echo -n " | " >> logs/08_comparison_table.txt
grep -c "WRITE:" logs/04_find_output.txt | tr -d '\n' >> logs/08_comparison_table.txt
echo -n " | " >> logs/08_comparison_table.txt
grep -c "CLOSE:" logs/04_find_output.txt | tr -d '\n' >> logs/08_comparison_table.txt
echo "" >> logs/08_comparison_table.txt

echo -n "TAR CREATE   | " >> logs/08_comparison_table.txt
grep -c "OPEN:" logs/05_tar_create_output.txt | tr -d '\n' >> logs/08_comparison_table.txt
echo -n " | " >> logs/08_comparison_table.txt
grep -c "OPENAT:" logs/05_tar_create_output.txt | tr -d '\n' >> logs/08_comparison_table.txt
echo -n " | " >> logs/08_comparison_table.txt
grep -c "READ:" logs/05_tar_create_output.txt | tr -d '\n' >> logs/08_comparison_table.txt
echo -n " | " >> logs/08_comparison_table.txt
grep -c "WRITE:" logs/05_tar_create_output.txt | tr -d '\n' >> logs/08_comparison_table.txt
echo -n " | " >> logs/08_comparison_table.txt
grep -c "CLOSE:" logs/05_tar_create_output.txt | tr -d '\n' >> logs/08_comparison_table.txt
echo "" >> logs/08_comparison_table.txt

echo -n "TAR EXTRACT  | " >> logs/08_comparison_table.txt
grep -c "OPEN:" logs/06_tar_extract_output.txt | tr -d '\n' >> logs/08_comparison_table.txt
echo -n " | " >> logs/08_comparison_table.txt
grep -c "OPENAT:" logs/06_tar_extract_output.txt | tr -d '\n' >> logs/08_comparison_table.txt
echo -n " | " >> logs/08_comparison_table.txt
grep -c "READ:" logs/06_tar_extract_output.txt | tr -d '\n' >> logs/08_comparison_table.txt
echo -n " | " >> logs/08_comparison_table.txt
grep -c "WRITE:" logs/06_tar_extract_output.txt | tr -d '\n' >> logs/08_comparison_table.txt
echo -n " | " >> logs/08_comparison_table.txt
grep -c "CLOSE:" logs/06_tar_extract_output.txt | tr -d '\n' >> logs/08_comparison_table.txt
echo "" >> logs/08_comparison_table.txt

echo -n "CP           | " >> logs/08_comparison_table.txt
grep -c "OPEN:" logs/07_cp_output.txt | tr -d '\n' >> logs/08_comparison_table.txt
echo -n " | " >> logs/08_comparison_table.txt
grep -c "OPENAT:" logs/07_cp_output.txt | tr -d '\n' >> logs/08_comparison_table.txt
echo -n " | " >> logs/08_comparison_table.txt
grep -c "READ:" logs/07_cp_output.txt | tr -d '\n' >> logs/08_comparison_table.txt
echo -n " | " >> logs/08_comparison_table.txt
grep -c "WRITE:" logs/07_cp_output.txt | tr -d '\n' >> logs/08_comparison_table.txt
echo -n " | " >> logs/08_comparison_table.txt
grep -c "CLOSE:" logs/07_cp_output.txt | tr -d '\n' >> logs/08_comparison_table.txt
echo "" >> logs/08_comparison_table.txt

cat logs/08_comparison_table.txt

# 6. Статическая программа
echo -e "\n=== 6. ТЕСТ СТАТИЧЕСКОЙ ПРОГРАММЫ ==="

cat > static_test.c << 'EOF'
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd = open("static_test.txt", O_CREAT | O_WRONLY, 0644);
    write(fd, "Hello from static program!", 26);
    close(fd);
    printf("Static program completed\n");
    return 0;
}
EOF

gcc -static -o static_test static_test.c

echo "--- Проверка статической программы ---"
file static_test > logs/09_static_file_check.txt
cat logs/09_static_file_check.txt
echo "Динамические зависимости:"
ldd static_test 2>&1 | head -3 > logs/10_static_ldd_check.txt
cat logs/10_static_ldd_check.txt

echo -e "\n--- Тест перехвата с LD_PRELOAD ---"
LD_PRELOAD=./libsyscall_spy.so ./static_test > logs/11_static_test_output.txt 2>&1
cat logs/11_static_test_output.txt

if grep -q "SPY" logs/11_static_test_output.txt; then
    echo "Перехват сработал!"
else
    echo "Перехват НЕ сработал!"
fi

# 7. Анализ системных файлов
echo -e "\n=== 7. АНАЛИЗ СИСТЕМНЫХ ВЫЗОВОВ ==="

echo -e "\n--- Системные файлы, открываемые FIND ---"
LD_PRELOAD=./libsyscall_spy.so find --version > /dev/null 2>&1
LD_PRELOAD=./libsyscall_spy.so find --version 2>&1 | grep -E "OPEN.*\.so|OPEN.*/etc|OPEN.*/proc" | head -10 > logs/12_system_files.txt
cat logs/12_system_files.txt

echo -e "\n--- Сравнение через strace ---"
echo "FIND:" > logs/13_strace_comparison.txt
strace -c find test_data -name "*.txt" 2>&1 | tail -5 >> logs/13_strace_comparison.txt
echo -e "\nCP:" >> logs/13_strace_comparison.txt
strace -c cp test_data/file1.txt test_copy.txt 2>&1 | tail -5 >> logs/13_strace_comparison.txt
cat logs/13_strace_comparison.txt

# 8. Создание README с описанием логов
echo -e "\n=== 8. СОЗДАНИЕ ОПИСАНИЯ ЛОГОВ ==="

cat > logs/README.md << 'EOF'
# Описание логов лабораторной работы 4

## Структура файлов:

### Основные логи программ:
- `01_file_check.txt` - проверка скомпилированной библиотеки
- `02_ldd_check.txt` - зависимости библиотеки
- `03_test_structure.txt` - структура тестовых данных
- `04_find_output.txt` - полный вывод программы find
- `05_tar_create_output.txt` - создание архива tar
- `06_tar_extract_output.txt` - распаковка архива tar
- `07_cp_output.txt` - копирование файлов cp

### Аналитические файлы:
- `08_comparison_table.txt` - сравнительная таблица вызовов
- `09_static_file_check.txt` - проверка статической программы
- `10_static_ldd_check.txt` - зависимости статической программы
- `11_static_test_output.txt` - тест перехвата статической программы
- `12_system_files.txt` - анализ системных файлов
- `13_strace_comparison.txt` - сравнение через strace

### Статистика по логам:
EOF

echo -e "\n### Размеры логов:" >> logs/README.md
ls -lh logs/*.txt | awk '{print "- " $9 " - " $5}' >> logs/README.md

echo -e "\n### Количество строк в основных логах:" >> logs/README.md
for log in 04 05 06 07; do
    count=$(wc -l < logs/${log}_*output.txt)
    echo "- ${log}_*output.txt - $count строк" >> logs/README.md
done

echo "README с описанием логов создан (logs/README.md)"
echo -e "\n=== 9. ОЧИСТКА ==="
rm -rf test_data extracted
rm -f *.txt *.tar static_test static_test.c
rm -f test_copy.txt
echo "Временные файлы удалены"

echo -e "\n=== ТЕСТ ЗАВЕРШЕН ==="
echo "Все данные собраны и сохранены в папке logs/"
echo "Логи программ: 04-07_*output.txt"
echo "Аналитика: 08-14_*.txt"
echo "Описание: logs/README.md"