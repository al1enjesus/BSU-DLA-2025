#!/bin/bash

# Запустить в Терминале 1

set -e  # Выход при ошибке

echo "=================================================="
echo "                  ЗАПУСК СИСТЕМЫ"
echo "=================================================="

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Функции для цветного вывода
log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Переменные
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CONFIG_FILE="config/config.json"
WORKERS=4
LOG_DIR="logs"
PID_DIR="pids"

cd "$PROJECT_DIR"

log_info "Текущая директория: $(pwd)"
log_info "Очистка предыдущих запусков..."
pkill -f "./supervisor" 2>/dev/null || true
pkill -f "./worker" 2>/dev/null || true
sleep 2
rm -rf "$PID_DIR" "$LOG_DIR"

log_info "Сборка проекта..."
if ! make build; then
    log_error "Ошибка сборки проекта"
    exit 1
fi

log_info "Создание директорий для логов и PID файлов..."
mkdir -p "$LOG_DIR" "$PID_DIR"

log_info "Запуск супервизора с $WORKERS воркерами..."
cd bin  

# Запускаем супервизор с логированием
./supervisor --config "$CONFIG_FILE" --workers $WORKERS > "../$LOG_DIR/supervisor.log" 2>&1 &
SUPERVISOR_PID=$!
cd ..

# Сохраняем PID супервизора
echo $SUPERVISOR_PID > "$PID_DIR/supervisor.pid"
log_success "Супервизор запущен с PID: $SUPERVISOR_PID"

log_info "Ожидание запуска воркеров..."
sleep 3

# Сохраняем PID всех воркеров
log_info "Сохранение PID воркеров..."
ps -eo pid,ppid,comm,args | awk "\$2 == $SUPERVISOR_PID && /worker/ {print \$1}" > "$PID_DIR/workers.pid"

WORKER_COUNT=$(wc -l < "$PID_DIR/workers.pid")
if [ $WORKER_COUNT -eq $WORKERS ]; then
    log_success "Запущено $WORKER_COUNT воркеров"
else
    log_warning "Запущено $WORKER_COUNT воркеров (ожидалось $WORKERS)"
fi

# Создаем индивидуальные PID файлы для каждого воркера
mkdir -p "$PID_DIR/workers"
i=0
while read pid; do
    if [ -n "$pid" ]; then
        echo $pid > "$PID_DIR/workers/worker_$i.pid"
        i=$((i+1))
    fi
done < "$PID_DIR/workers.pid"

echo ""
echo "=== ИНФОРМАЦИЯ О ПРОЦЕССАХ ==="
ps -p $SUPERVISOR_PID,$(cat "$PID_DIR/workers.pid" | tr '\n' ',' | sed 's/,$//') -o pid,ppid,ni,psr,pcpu,pmem,comm,args

echo ""
echo "(Терминал 2): ./scripts/monitor.sh"
echo "(Терминал 3): ./scripts/control.sh"
echo "Остановка: kill -TERM $SUPERVISOR_PID"
echo ""

echo "Логи: $LOG_DIR/supervisor.log"
echo "PID файлы: $PID_DIR/"

echo "Супервизор и $WORKER_COUNT воркеров запущены успешно."
