#!/bin/bash

echo "=== Сравнительный тест pstat с /proc и системными утилитами ==="

# Сборка утилиты
echo -e "\n1. Сборка pstat..."
cd src && make && cd ..

# Выбор PID для тестирования (по умолчанию текущий процесс)
TARGET_PID=$$
if [ $# -eq 1 ]; then
    TARGET_PID=$1
fi

echo -e "\n2. Тестируем PID: $TARGET_PID"

# Проверка существования процесса
if [ ! -d "/proc/$TARGET_PID" ]; then
    echo "Ошибка: Процесс $TARGET_PID не существует!"
    exit 1
fi

echo -e "\n3. Запуск pstat..."
src/pstat $TARGET_PID

echo -e "\n4. Сравнение с /proc файлами..."

echo -e "\n=== /proc/$TARGET_PID/status ==="
grep -E "(Pid|PPid|State|Threads|VmRSS|RssAnon|RssFile|voluntary|nonvoluntary)" /proc/$TARGET_PID/status 2>/dev/null || echo "Нет доступа к status"

echo -e "\n=== /proc/$TARGET_PID/stat ==="
cat /proc/$TARGET_PID/stat 2>/dev/null | awk '{
    print "PID: " $1
    print "Comm: " $2
    print "State: " $3
    print "PPid: " $4
    print "utime: " $14
    print "stime: " $15
    print "Threads: " $20
}' || echo "Нет доступа к stat"

echo -e "\n=== /proc/$TARGET_PID/io ==="
if [ -r /proc/$TARGET_PID/io ]; then
    grep -E "(read_bytes|write_bytes)" /proc/$TARGET_PID/io
else
    echo "Нет доступа к /proc/$TARGET_PID/io"
fi

echo -e "\n5. Сравнение с системными утилитами..."

echo -e "\n=== ps ==="
ps -p $TARGET_PID -o pid,ppid,state,utime,stime,thcount,vsize,rss,comm,time 2>/dev/null || echo "Ошибка ps"

echo -e "\n=== top (одно снятие) ==="
top -b -n 1 -p $TARGET_PID 2>/dev/null | head -20 || echo "Ошибка top"

echo -e "\n=== pidstat ==="
if command -v pidstat >/dev/null 2>&1; then
    pidstat -p $TARGET_PID 1 1 2>/dev/null || echo "Ошибка pidstat"
else
    echo "pidstat не установлен"
fi

echo -e "\n6. Детальное сравнение значений..."

# Получаем значения из pstat для автоматического сравнения
echo -e "\n=== Автоматическое сравнение ==="

# Получаем значения из /proc
PROC_PPID=$(awk '{print $4}' /proc/$TARGET_PID/stat 2>/dev/null)
PROC_STATE=$(awk '{print $3}' /proc/$TARGET_PID/stat 2>/dev/null)
PROC_THREADS=$(awk '{print $20}' /proc/$TARGET_PID/stat 2>/dev/null)
PROC_UTIME=$(awk '{print $14}' /proc/$TARGET_PID/stat 2>/dev/null)
PROC_STIME=$(awk '{print $15}' /proc/$TARGET_PID/stat 2>/dev/null)

# Получаем значения из status
PROC_VMRSS=$(grep "VmRSS:" /proc/$TARGET_PID/status 2>/dev/null | awk '{print $2}')
PROC_RSSANON=$(grep "RssAnon:" /proc/$TARGET_PID/status 2>/dev/null | awk '{print $2}')
PROC_RSSFILE=$(grep "RssFile:" /proc/$TARGET_PID/status 2>/dev/null | awk '{print $2}')
PROC_VOL_CTXT=$(grep "voluntary_ctxt_switches:" /proc/$TARGET_PID/status 2>/dev/null | awk '{print $2}')
PROC_NONVOL_CTXT=$(grep "nonvoluntary_ctxt_switches:" /proc/$TARGET_PID/status 2>/dev/null | awk '{print $2}')

# Получаем значения из io
if [ -r /proc/$TARGET_PID/io ]; then
    PROC_READ_BYTES=$(grep "read_bytes:" /proc/$TARGET_PID/io | awk '{print $2}')
    PROC_WRITE_BYTES=$(grep "write_bytes:" /proc/$TARGET_PID/io | awk '{print $2}')
else
    PROC_READ_BYTES="N/A"
    PROC_WRITE_BYTES="N/A"
fi

# Вычисляем CPU время без bc
HZ=100
PROC_CPU_TIME=$(echo "scale=2; ($PROC_UTIME + $PROC_STIME) / $HZ" | awk '{printf "%.2f", $1}' 2>/dev/null)

echo "Сравнительная таблица:"
echo "======================"
printf "%-30s | %-15s | %-15s\n" "Метрика" "/proc" "pstat"
printf "%-30s | %-15s | %-15s\n" "------------------------------" "---------------" "---------------"

compare_field() {
    local field_name="$1"
    local proc_value="$2"
    local pstat_line="$3"
    
    printf "%-30s | %-15s | " "$field_name" "$proc_value"
    src/pstat $TARGET_PID 2>/dev/null | grep "$pstat_line" | awk -F: '{print $2}' | xargs
}

compare_field "PPid" "$PROC_PPID" "PPid:"
compare_field "State" "$PROC_STATE" "State:"
compare_field "Threads" "$PROC_THREADS" "Threads:"
compare_field "utime (тики)" "$PROC_UTIME" "utime:"
compare_field "stime (тики)" "$PROC_STIME" "stime:"
compare_field "CPU time (сек)" "$PROC_CPU_TIME" "CPU time"

# Сравнение памяти
compare_field "VmRSS (kB)" "$PROC_VMRSS" "VmRSS:"
compare_field "RssAnon (kB)" "$PROC_RSSANON" "RssAnon:"
compare_field "RssFile (kB)" "$PROC_RSSFILE" "RssFile:"

# Сравнение переключений контекста
compare_field "voluntary_ctxt_switches" "$PROC_VOL_CTXT" "voluntary_ctxt_switches:"
compare_field "nonvoluntary_ctxt_switches" "$PROC_NONVOL_CTXT" "nonvoluntary_ctxt_switches:"

# Сравнение IO
if [ "$PROC_READ_BYTES" != "N/A" ]; then
    compare_field "read_bytes" "$PROC_READ_BYTES" "read_bytes:"
    compare_field "write_bytes" "$PROC_WRITE_BYTES" "write_bytes:"
fi

echo -e "\n7. Проверка форматирования..."
echo "Проверяем, что pstat корректно форматирует размеры:"
src/pstat $TARGET_PID 2>/dev/null | grep -E "(MiB|KiB|VmRSS|RssAnon|RssFile|read_bytes|write_bytes)" || echo "Ошибка запуска pstat"

echo -e "\n8. Проверка вычисления CPU времени..."
echo "utime + stime = $PROC_UTIME + $PROC_STIME = $(($PROC_UTIME + $PROC_STIME)) тиков"
echo "CPU time = ($PROC_UTIME + $PROC_STIME) / $HZ = $PROC_CPU_TIME сек"

echo -e "\n=== Тест завершен ==="
