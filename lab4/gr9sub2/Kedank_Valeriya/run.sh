set -e

echo "Задание А: LD_PRELOAD перехват системных вызовов"
echo "Программы: gcc, make, as (вариант 3)"

cd src

# Создаем директории для логов
mkdir -p ../logs
rm -f ../logs/*.log

# Создаем тестовые файлы для экспериментов
echo "Создание тестовых файлов..."
echo "int main() { return 0; }" > simple.c
echo "test: simple.c" > Makefile.test
echo -e "\tgcc -o simple simple.c" >> Makefile.test

run_experiment() {
    local program_name=$1
    local command=$2
    local log_file="../logs/${program_name}.log"
    
    echo ""
    echo "=== Эксперимент: $program_name ==="
    echo "Команда: $command"
    echo "Лог файл: $log_file"
    
    LD_PRELOAD=./libsyscall_spy.so $command > /dev/null 2> "$log_file"
    
    local total_calls=$(grep -c "\[SPY\]" "$log_file")
    local open_calls=$(grep -c "\[SPY\] open" "$log_file")
    local openat_calls=$(grep -c "\[SPY\] openat" "$log_file")
    local read_calls=$(grep -c "\[SPY\] read" "$log_file")
    local write_calls=$(grep -c "\[SPY\] write" "$log_file")
    local close_calls=$(grep -c "\[SPY\] close" "$log_file")
    
    echo "Статистика для $program_name:"
    echo "  Всего системных вызовов: $total_calls"
    echo "  open(): $open_calls"
    echo "  openat(): $openat_calls" 
    echo "  read(): $read_calls"
    echo "  write(): $write_calls"
    echo "  close(): $close_calls"
    
    echo "$program_name,$total_calls,$open_calls,$openat_calls,$read_calls,$write_calls,$close_calls" >> ../logs/stats.csv
}

echo "program,total,open,openat,read,write,close" > ../logs/stats.csv

echo ""
echo "=== ЗАПУСК ЭКСПЕРИМЕНТОВ ==="

# Эксперимент 1: gcc --version (простая версия)
run_experiment "gcc_version" "gcc --version"

# Эксперимент 2: gcc компиляция
run_experiment "gcc_compile" "gcc -c simple.c -o simple.o"

# Эксперимент 3: make --version  
run_experiment "make_version" "make --version"

# Эксперимент 4: make сборка
run_experiment "make_build" "make -f Makefile.test"

# Эксперимент 5: as --version
run_experiment "as_version" "as --version"

# Эксперимент 6: as ассемблирование (упрощенный тест)
run_experiment "as_assemble" "echo '.text; .globl main; main: ret' | as -o simple_asm.o - 2>/dev/null || true"

echo ""
echo "=== ДЕТАЛЬНЫЙ АНАЛИЗ РЕЗУЛЬТАТОВ ==="

for program in gcc_compile make_build as_assemble; do
    log_file="../logs/${program}.log"
    
    if [ -f "$log_file" ]; then
        echo ""
        echo "--- Анализ $program ---"
        
        echo "Распределение вызовов:"
        grep "\[SPY\]" "$log_file" | awk '{print $2}' | sort | uniq -c | sort -rn
        
        echo ""
        echo "Часто открываемые файлы:"
        grep "open" "$log_file" | grep "path=" | sed 's/.*path="\([^"]*\)".*/\1/' | sort | uniq -c | sort -rn | head -5
        
        echo ""
        echo "Примеры вызовов:"
        grep "\[SPY\]" "$log_file" | head -3
    fi
done

echo ""
echo "=== СРАВНИТЕЛЬНЫЙ АНАЛИЗ ==="
echo "Сравнение программ по количеству системных вызовов:"
if [ -f "../logs/stats.csv" ]; then
    column -t -s, ../logs/stats.csv
fi

echo ""
echo "=== ТЕСТ СТАТИЧЕСКОЙ КОМПИЛЯЦИИ ==="
echo "Компиляция статической программы..."
gcc -static -o test_program_static test_program.c

echo "Запуск статической программы с LD_PRELOAD:"
LD_PRELOAD=./libsyscall_spy.so ./test_program_static 2>&1 | head -5

if [ ${PIPESTATUS[0]} -eq 0 ]; then
    echo "Результат: перехват НЕ сработал (программа выполнилась без перехвата)"
else
    echo "Результат: перехват сработал частично"
fi

echo ""
echo "=================================================="
echo "Эксперименты завершены!"
echo "Логи сохранены в директории ../logs/"
echo "Для просмотра детальных логов:"
echo "  less ../logs/gcc_compile.log"
echo "  less ../logs/make_build.log"
echo "=================================================="

cd ..
