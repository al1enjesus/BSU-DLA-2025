#!/bin/bash

# === НАСТРОЙКИ И ЦВЕТА ===
SUPERVISOR="src/supervisor.py"
CONFIG_FILE="src/config.json"
BACKUP_CONFIG="src/config.json.bak"

GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# Проверка прав root
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}[ERROR] Запустите скрипт через sudo!${NC}"
    exit 1
fi

# Функция очистки
cleanup() {
    echo -e "\n${BLUE}[CLEANUP] Завершение процессов...${NC}"
    pkill -f "lab2-supervisor" 2>/dev/null
    pkill -f "lab2-worker" 2>/dev/null
    if [ -f "$BACKUP_CONFIG" ]; then mv "$BACKUP_CONFIG" "$CONFIG_FILE"; fi
    echo "Done."
}
trap cleanup EXIT

echo -e "${BLUE}==========================================================${NC}"
echo -e "${BLUE}    ПОЛНАЯ ДЕМОНСТРАЦИЯ ЗАДАНИЯ B (Affinity & Nice)       ${NC}"
echo -e "${BLUE}==========================================================${NC}"

# ---------------------------------------------------------
# ЭТАП 1: Начальная конфигурация
# ---------------------------------------------------------
echo -e "\n${YELLOW}[STEP 1] Генерация конфига и запуск...${NC}"
echo "Схема эксперимента:"
echo "  CPU 0: Worker-0 (Nice 0) vs Worker-1 (Nice 10) -> Ожидаем НЕРАВЕНСТВО"
echo "  CPU 1: Worker-2 (Nice 0) vs Worker-3 (Nice 0)  -> Ожидаем РАВЕНСТВО (50/50)"

cp "$CONFIG_FILE" "$BACKUP_CONFIG"
cat > "$CONFIG_FILE" <<EOF
{
    "workers_count": 4,
    "restart_window_sec": 30,
    "max_restarts": 10,
    "mode_heavy": { "work_sec": 0.5, "sleep_sec": 0.1 },
    "mode_light": { "work_sec": 0.05, "sleep_sec": 0.5 },
    "scheduling": [
        {"worker_id": 0, "nice": 0,  "cpu_affinity": [0]},
        {"worker_id": 1, "nice": 10, "cpu_affinity": [0]},
        {"worker_id": 2, "nice": 0,  "cpu_affinity": [1]},
        {"worker_id": 3, "nice": 0,  "cpu_affinity": [1]}
    ]
}
EOF

# Запуск
python3 $SUPERVISOR > /dev/null 2>&1 &
SUPERVISOR_PID=$!
sleep 3
# Включаем нагрузку
kill -SIGUSR2 $SUPERVISOR_PID
sleep 2

# Получаем PIDы конкретных воркеров по именам (спасибо setproctitle)
W0=$(pgrep -f "lab2-worker-0")
W1=$(pgrep -f "lab2-worker-1")
W2=$(pgrep -f "lab2-worker-2")
W3=$(pgrep -f "lab2-worker-3")

# ---------------------------------------------------------
# ЭТАП 2: Сбор начальной статистики
# ---------------------------------------------------------
echo -e "\n${YELLOW}[STEP 2] Сбор статистики (Базовая линия)...${NC}"
echo "Замеряем pidstat (5 секунд)..."
echo -e "${CYAN}Следите за %CPU:${NC}"

# Выводим pidstat, подсвечиваем строки воркеров
pidstat -p "$W0,$W1,$W2,$W3" -u 1 5

echo -e "\n${GREEN}>>> АНАЛИЗ:${NC}"
echo "  На CPU 0: Worker-0 должен иметь ~65-70%, Worker-1 ~30% (из-за разницы nice)."
echo "  На CPU 1: Worker-2 и Worker-3 должны иметь по ~50% (равный nice)."

# ---------------------------------------------------------
# ЭТАП 3: Проверка CPU Affinity
# ---------------------------------------------------------
echo -e "\n${YELLOW}[STEP 3] Проверка CPU Affinity (Привязка к ядрам)${NC}"
echo "Используем 'ps' для проверки столбца PSR (Processor ID):"

ps -o pid,comm,psr,ni -p "$W0,$W1,$W2,$W3"

echo -e "${GREEN}>>> ПРОВЕРКА:${NC}"
echo "  Worker 0, 1 должны быть на PSR 0."
echo "  Worker 2, 3 должны быть на PSR 1."

# ---------------------------------------------------------
# ЭТАП 4: Демонстрация Real-time Renice
# ---------------------------------------------------------
echo -e "\n${YELLOW}[STEP 4] Демонстрация изменения Nice в реальном времени${NC}"
echo "Мы берем пару с CPU 1 (Worker-2 и Worker-3), которые сейчас равны."
echo -e "${RED}ДЕЙСТВИЕ: ${NC}"
echo "  1. Worker-2: Nice 0 -> Nice 19 (Минимальный приоритет)"
echo "  2. Worker-3: Nice 0 -> Nice -10 (Высокий приоритет)"

renice -n 19 -p $W2 > /dev/null
renice -n -10 -p $W3 > /dev/null

echo -e "Приоритеты изменены. Замеряем влияние (5 секунд)..."

pidstat -p "$W2,$W3" -u 1 5

echo -e "\n${GREEN}>>> ВЫВОД:${NC}"
echo "  Worker-3 должен захватить почти 90-95% ядра CPU 1."
echo "  Worker-2 должен остаться с минимумом ресурсов."

echo -e "\n${BLUE}Демонстрация успешно завершена.${NC}"