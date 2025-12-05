#!/bin/bash

# Цвета для вывода
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

CONFIG_FILE="src/config.json"
SUPERVISOR_SCRIPT="src/supervisor.py"

# Функция для вывода заголовков этапов
print_step() {
    echo -e "\n${BLUE}======================================================${NC}"
    echo -e "${YELLOW}ШАГ: $1${NC}"
    echo -e "${BLUE}======================================================${NC}"
}

# Функция для отображения текущего состояния процессов
show_status() {
    echo -e "${GREEN}[STATUS] Дерево процессов и потребление ресурсов:${NC}"
    if [ -z "$SUPERVISOR_PID" ]; then
        echo "Супервизор не запущен."
        return
    fi
    
    # pstree показывает иерархию, ps показывает %CPU и статус
    # Используем pstree если есть, иначе ps --forest
    if command -v pstree &> /dev/null; then
        pstree -p -a -g $SUPERVISOR_PID | sed 's/^/    /'
    fi
    
    echo -e "${GREEN}[STATS] Детально (PID, PPID, STATE, %CPU, CMD):${NC}"
    # Выводим супервизора и всех его детей
    ps -o pid,ppid,state,%cpu,comm --forest -g $(ps -o sid= -p $SUPERVISOR_PID | tr -d ' ') | grep -v "ps" | head -n 10
}

# Очистка при выходе (Ctrl+C или конец скрипта)
cleanup() {
    echo -e "\n${RED}[CLEANUP] Восстановление конфига и завершение процессов...${NC}"
    # Восстанавливаем конфиг из бэкапа
    if [ -f "${CONFIG_FILE}.bak" ]; then
        mv "${CONFIG_FILE}.bak" "${CONFIG_FILE}"
    fi
    
    # Убиваем супервизора, если жив
    if [ ! -z "$SUPERVISOR_PID" ]; then
        kill -SIGTERM $SUPERVISOR_PID 2>/dev/null
        wait $SUPERVISOR_PID 2>/dev/null
    fi
}
trap cleanup EXIT

# ================= НАЧАЛО ДЕМОНСТРАЦИИ =================

# 0. Подготовка
if [ ! -f "${CONFIG_FILE}.bak" ]; then
    cp "$CONFIG_FILE" "${CONFIG_FILE}.bak"
fi

print_step "1. Запуск Супервизора"
echo "Запускаем python3 $SUPERVISOR_SCRIPT в фоне..."
python3 $SUPERVISOR_SCRIPT &
SUPERVISOR_PID=$!
echo "PID Супервизора: $SUPERVISOR_PID"

sleep 2 # Даем время на инициализацию
show_status

print_step "2. Переключение режимов (Сигналы SIGUSR)"
echo "Отправляем SIGUSR1 (Переход в режим LIGHT - мало CPU)..."
kill -SIGUSR1 $SUPERVISOR_PID
sleep 2
# В реальном top было бы видно падение CPU, тут видим лог
show_status

echo -e "\nОтправляем SIGUSR2 (Переход в режим HEAVY - много CPU)..."
kill -SIGUSR2 $SUPERVISOR_PID
sleep 2
show_status

print_step "3. Устойчивость (Убийство воркера)"
# Получаем PID первого попавшегося воркера
WORKER_PID=$(pgrep -P $SUPERVISOR_PID | head -n 1)

if [ -z "$WORKER_PID" ]; then
    echo "Ошибка: Воркеры не найдены!"
    exit 1
fi

echo -e "Убиваем воркера PID: ${RED}$WORKER_PID${NC} (kill -9)"
kill -9 $WORKER_PID
sleep 1

echo "Проверяем, что PID изменился (Супервизор должен был перезапустить воркера):"
show_status

print_step "4. Rate Limiting (Защита от Fork Bomb)"
echo "Сейчас будем убивать воркеров очень быстро (6 раз)..."
echo "Ожидаем, что супервизор перестанет их поднимать."

for i in {1..6}; do
    CURRENT_WORKER=$(pgrep -P $SUPERVISOR_PID | head -n 1)
    if [ ! -z "$CURRENT_WORKER" ]; then
        kill -9 $CURRENT_WORKER
        echo "Killed $CURRENT_WORKER"
    fi
    sleep 0.2
done

sleep 2
echo "Результат (количество воркеров должно уменьшиться или стать 0):"
show_status

print_step "5. Горячая перезагрузка (SIGHUP)"
echo "Изменяем конфиг 'на лету': workers_count 3 -> 5"

# Используем sed для замены значения в JSON (примитивный парсинг)
sed -i 's/"workers_count": [0-9]*/"workers_count": 5/' "$CONFIG_FILE"

echo "Отправляем SIGHUP..."
kill -SIGHUP $SUPERVISOR_PID
sleep 3

echo "Проверяем, что воркеров стало 5:"
show_status

print_step "6. Корректное завершение (Graceful Shutdown)"
echo "Отправляем SIGTERM..."
kill -SIGTERM $SUPERVISOR_PID

# Ждем завершения процесса
wait $SUPERVISOR_PID
echo -e "${GREEN}Супервизор завершил работу.${NC}"

echo -e "\nПроверка, что процессов не осталось:"
if pgrep -f "lab2-supervisor" > /dev/null; then
    echo -e "${RED}ОШИБКА: Процессы остались висеть!${NC}"
    pgrep -a -f "lab2"
else
    echo -e "${GREEN}Чисто. Все процессы завершены.${NC}"
fi

# Trap cleanup выполнится автоматически и вернет конфиг на место