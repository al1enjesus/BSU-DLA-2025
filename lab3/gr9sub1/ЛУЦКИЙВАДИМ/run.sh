#!/bin/bash

echo "🔧 Лабораторная работа 3 - Утилита pstat"
echo "========================================"

# Проверяем Python
if ! command -v python3 &> /dev/null; then
    echo "❌ Ошибка: python3 не найден"
    exit 1
fi

# Делаем скрипт исполняемым
chmod +x src/pstat.py

# PID текущего процесса
CURRENT_PID=$$

echo ""
echo "1. 🧪 ТЕСТ УТИЛИТЫ PSTAT НА ТЕКУЩЕМ ПРОЦЕССЕ (PID: $$)"
echo "------------------------------------------------------"
python3 src/pstat.py $$

echo ""
echo "2. 📊 СРАВНЕНИЕ С СИСТЕМНЫМИ УТИЛИТАМИ"
echo "--------------------------------------"

# Получаем точные данные из нашего pstat для сравнения
echo ""
echo "🔍 Получение метрик для точного сравнения..."
PSTAT_OUTPUT=$(python3 src/pstat.py $$ 2>/dev/null)

# Извлекаем конкретные значения из вывода pstat
PSTAT_PPID=$(echo "$PSTAT_OUTPUT" | grep "Родительский PID" | awk '{print $4}')
PSTAT_THREADS=$(echo "$PSTAT_OUTPUT" | grep "Количество потоков" | awk '{print $3}')
PSTAT_STATE=$(echo "$PSTAT_OUTPUT" | grep "Состояние" | awk '{print $2}')
PSTAT_MEMORY=$(echo "$PSTAT_OUTPUT" | grep "Физическая память" | awk '{print $3}')
PSTAT_CPU_TIME=$(echo "$PSTAT_OUTPUT" | grep "Общее время CPU" | awk '{print $4}')

# Получаем данные из ps
PS_DATA=$(ps -p $$ -o pid,ppid,state,utime,stime,nlwp,vsz,rss --no-headers 2>/dev/null)
if [ -n "$PS_DATA" ]; then
    PS_PPID=$(echo "$PS_DATA" | awk '{print $2}')
    PS_STATE=$(echo "$PS_DATA" | awk '{print $3}')
    PS_THREADS=$(echo "$PS_DATA" | awk '{print $6}')
    PS_RSS=$(echo "$PS_DATA" | awk '{print $8}')
    PS_MEMORY_MB=$(echo "scale=2; $PS_RSS / 1024" | bc 2>/dev/null || echo "?")
else
    PS_PPID="N/A"
    PS_STATE="N/A"
    PS_THREADS="N/A"
    PS_MEMORY_MB="N/A"
fi

# Получаем данные из top
TOP_DATA=$(top -b -n 1 -p $$ 2>/dev/null | grep -w $$)
if [ -n "$TOP_DATA" ]; then
    TOP_PID=$(echo "$TOP_DATA" | awk '{print $1}')
    TOP_MEMORY_KB=$(echo "$TOP_DATA" | awk '{print $6}')
    TOP_STATE=$(echo "$TOP_DATA" | awk '{print $8}')
    TOP_MEMORY_MB=$(echo "scale=2; $TOP_MEMORY_KB / 1024" | bc 2>/dev/null || echo "?")
else
    TOP_PID="N/A"
    TOP_MEMORY_MB="N/A"
    TOP_STATE="N/A"
fi

# Получаем данные из pidstat
PIDSTAT_DATA=$(pidstat -p $$ 1 1 2>/dev/null | grep -w $$ | tail -1)
if [ -n "$PIDSTAT_DATA" ]; then
    PIDSTAT_CPU=$(echo "$PIDSTAT_DATA" | awk '{print $8}')
else
    PIDSTAT_CPU="N/A"
fi

echo ""
echo "📋 ТАБЛИЦА СРАВНЕНИЯ МЕТРИК ПРОЦЕССА $$:"
echo ""

# Создаем временный файл для таблицы
TABLE_FILE=$(mktemp)
cat > "$TABLE_FILE" << EOF
+----------------+-----------+-----------+-------------+------------+----------------+--------------+
| Утилита        | PID       | PPid      | Состояние   | Потоки     | Память RSS     | CPU время    |
+----------------+-----------+-----------+-------------+------------+----------------+--------------+
| pstat          | $$        | $PSTAT_PPID       | $PSTAT_STATE          | $PSTAT_THREADS          | $PSTAT_MEMORY     | $PSTAT_CPU_TIME     |
| ps             | $$        | $PS_PPID       | $PS_STATE          | $PS_THREADS          | ${PS_MEMORY_MB} MiB     | -           |
| top            | $$        | -         | $TOP_STATE          | -          | ${TOP_MEMORY_MB} MiB     | -           |
| pidstat        | $$        | -         | -           | -          | -              | ${PIDSTAT_CPU}% CPU |
+----------------+-----------+-----------+-------------+------------+----------------+--------------+
EOF

# Выводим таблицу с помощью column для красивого форматирования
if command -v column &> /dev/null; then
    column -t -s '|' "$TABLE_FILE"
else
    cat "$TABLE_FILE"
fi
rm "$TABLE_FILE"

echo ""
echo "3. 📁 ПРОВЕРКА ДОСТУПНОСТИ ФАЙЛОВ /proc"
echo "---------------------------------------"
for file in stat status io smaps_rollup smaps; do
    if [ -f "/proc/$$/$file" ] && [ -r "/proc/$$/$file" ]; then
        echo "✅ /proc/$$/$file доступен"
        # Покажем размер файла для информации
        if [ "$file" = "smaps" ] || [ "$file" = "smaps_rollup" ]; then
            FILE_SIZE=$(stat -c%s "/proc/$$/$file" 2>/dev/null || echo "?")
            echo "   Размер: $FILE_SIZE байт"
        fi
    else
        echo "❌ /proc/$$/$file недоступен"
    fi
done

echo ""
echo "4. 🔍 ДЕТАЛЬНЫЙ АНАЛИЗ РАСХОЖДЕНИЙ"
echo "----------------------------------"
echo "📊 Анализ памяти:"
echo "   - pstat:  $PSTAT_MEMORY (из VmRSS)"
echo "   - ps:     ${PS_MEMORY_MB} MiB (из RSS)"
echo "   - top:    ${TOP_MEMORY_MB} MiB (из RES)"
echo ""
echo "📈 Анализ состояния:"
echo "   - pstat: $PSTAT_STATE"
echo "   - ps:    $PS_STATE" 
echo "   - top:   $TOP_STATE"
echo ""
echo "🔄 Анализ потоков:"
echo "   - pstat: $PSTAT_THREADS"
echo "   - ps:    $PS_THREADS"

echo ""
echo "5. 🛠️  ДОПОЛНИТЕЛЬНАЯ ДИАГНОСТИКА"
echo "---------------------------------"
echo "💡 Проверка контекстных переключений:"
if [ -f "/proc/$$/status" ]; then
    VOLUNTARY=$(grep "voluntary_ctxt_switches" "/proc/$$/status" | awk '{print $2}')
    NONVOLUNTARY=$(grep "nonvoluntary_ctxt_switches" "/proc/$$/status" | awk '{print $2}')
    echo "   Добровольные: $VOLUNTARY"
    echo "   Принудительные: $NONVOLUNTARY"
fi

echo ""
echo "💡 Проверка ввода-вывода:"
if [ -f "/proc/$$/io" ]; then
    READ_BYTES=$(grep "read_bytes" "/proc/$$/io" | awk '{print $2}')
    WRITE_BYTES=$(grep "write_bytes" "/proc/$$/io" | awk '{print $2}')
    echo "   Прочитано: $READ_BYTES байт"
    echo "   Записано: $WRITE_BYTES байт"
fi

echo ""
echo "🎉 ДЕМОНСТРАЦИЯ ЗАВЕРШЕНА!"
echo "📖 Полный анализ смотрите в REPORT.MD"