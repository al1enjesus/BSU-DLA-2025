#!/bin/bash

# Запустить в Терминале 2

set -e

echo "=================================================="
echo "                   МОНИТОРИНГ"
echo "=================================================="

# Цвета для вывода
BLUE='\033[0;34m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$PROJECT_DIR"

# Проверяем что супервизор запущен
if [ ! -f "pids/supervisor.pid" ]; then  
    echo "Супервизор не запущен. Сначала выполните: ./scripts/setup.sh"
    exit 1
fi

SUPERVISOR_PID=$(cat "pids/supervisor.pid")

# Проверяем что супервизор жив
if ! ps -p $SUPERVISOR_PID > /dev/null 2>&1; then
    log_warning "Супервизор (PID: $SUPERVISOR_PID) не запущен"
    exit 1
fi

echo "Супервизор PID: $SUPERVISOR_PID"
echo "Для выхода: Ctrl+C"
echo ""

# Создаем временный скрипт для watch чтобы избежать проблем с экранированием
TEMP_SCRIPT=$(mktemp)
cat > "$TEMP_SCRIPT" << 'EOF'
#!/bin/bash
SUPERVISOR_PID=$1

echo "=== ДИНАМИЧЕСКИЙ МОНИТОРИНГ ПРОЦЕССОВ ==="
echo "Супервизор PID: $SUPERVISOR_PID"
echo "Обновлено: $(date)"
echo ""

# Находим все воркеры этого супервизора
WORKER_PIDS=$(ps -eo pid,ppid,comm,args | awk "\$2 == $SUPERVISOR_PID && /worker/ {print \$1}" | tr '\n' ',' | sed 's/,$//')

if [ -n "$WORKER_PIDS" ]; then
    ALL_PIDS="$SUPERVISOR_PID,$WORKER_PIDS"
    echo "Все PIDs: $ALL_PIDS"
    echo ""
    ps -p $ALL_PIDS -o pid,ppid,ni,psr,pcpu,pmem,comm,args 2>/dev/null || echo "Некоторые процессы завершились"
else
    echo "Воркеры не найдены"
    ps -p $SUPERVISOR_PID -o pid,ppid,ni,psr,pcpu,pmem,comm,args 2>/dev/null
fi

echo ""
echo "=== СТАТУС ==="
if [ -n "$WORKER_PIDS" ]; then
    WORKER_COUNT=$(echo "$WORKER_PIDS" | tr ',' '\n' | wc -l)
    echo "Супервизор + $WORKER_COUNT воркеров"
else
    echo "Только супервизор (воркеры завершены)"
fi
EOF

chmod +x "$TEMP_SCRIPT"

# Запускаем мониторинг
watch -n 1 "$TEMP_SCRIPT $SUPERVISOR_PID"

# Удаляем временный скрипт при завершении
trap "rm -f '$TEMP_SCRIPT'" EXIT
