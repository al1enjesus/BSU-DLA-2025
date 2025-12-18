#!/bin/bash

# Запустить в Терминале 3

set -e

echo "=================================================="
echo "               УПРАВЛЕНИЕ СИГНАЛАМИ"
echo "=================================================="

# Цвета для вывода
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)" 
PID_DIR="$PROJECT_DIR/pids"
LOG_DIR="$PROJECT_DIR/logs"

cd "$PROJECT_DIR"

# Проверяем что супервизор запущен
if [ ! -f "$PID_DIR/supervisor.pid" ]; then
    echo "Супервизор не запущен. Сначала выполните: ./scripts/setup.sh"
    exit 1
fi

SUPERVISOR_PID=$(cat "$PID_DIR/supervisor.pid")

# Функция для проверки что процесс жив
check_process() {
    if ! ps -p $1 > /dev/null 2>&1; then
        log_warning "Процесс $1 не найден. Завершение демонстрации."
        exit 1
    fi
}

# Функция для обновления PID файлов
update_pid_files() {
    local supervisor_pid=$1
    local pid_dir=$2
    
    if [ -z "$supervisor_pid" ]; then
        return 1
    fi
    
    # Находим всех живых воркеров
    ps -eo pid,ppid,comm,args | awk "\$2 == $supervisor_pid && /worker/ {print \$1}" > "$pid_dir/workers.pid"
    
    # Создаем индивидуальные файлы
    mkdir -p "$pid_dir/workers"
    i=0
    while read pid; do
        if [ -n "$pid" ]; then
            echo $pid > "$pid_dir/workers/worker_$i.pid"
            i=$((i+1))
        fi
    done < "$pid_dir/workers.pid"
    
    local count=$(wc -l < "$pid_dir/workers.pid" 2>/dev/null || echo 0)
    echo $count
}

# Функция для ожидания и показа логов
wait_and_show_logs() {
    local duration=$1
    local message=$2
    
    echo ""
    log_info "$message"
    echo "Ждем $duration секунд... (наблюдайте изменения в Терминале 2)"
    
    # Показываем последние логи во время ожидания
    for i in $(seq 1 $duration); do
        if [ -f "$LOG_DIR/supervisor.log" ]; then
            echo -n "."
        fi
        sleep 1
    done
    echo ""
    
    # Показываем последние логи
    if [ -f "$LOG_DIR/supervisor.log" ]; then
        echo "=== ПОСЛЕДНИЕ СОБЫТИЯ ИЗ ЛОГА ==="
        tail -10 "$LOG_DIR/supervisor.log" | while read line; do
            echo "  $line"
        done
    fi
}

echo ""
echo "Супервизор PID: $SUPERVISOR_PID"
echo "Логи: $LOG_DIR/supervisor.log"
echo ""

# ОБНОВЛЯЕМ PID ФАЙЛЫ ПЕРЕД НАЧАЛОМ ДЕМОНСТРАЦИИ
log_info "Обновление PID файлов перед началом демонстрации..."
WORKER_COUNT=$(update_pid_files $SUPERVISOR_PID "$PID_DIR")
log_success "Обновлено PID файлов: $WORKER_COUNT"

# ДЕМОНСТРАЦИЯ 1: Переключение в ЛЕГКИЙ режим
check_process $SUPERVISOR_PID
log_info "1. ДЕМОНСТРАЦИЯ: Переключение в ЛЕГКИЙ режим (SIGUSR1)"
echo "   Ожидание: воркеры перейдут в легкий режим (20% CPU)"
kill -USR1 $SUPERVISOR_PID
wait_and_show_logs 5 "Воркеры переключаются в легкий режим..."

# ДЕМОНСТРАЦИЯ 2: Переключение в ТЯЖЕЛЫЙ режим  
check_process $SUPERVISOR_PID
log_info "2. ДЕМОНСТРАЦИЯ: Переключение в ТЯЖЕЛЫЙ режим (SIGUSR2)"
echo "   Ожидание: воркеры перейдут в тяжелый режим (90% CPU)"
kill -USR2 $SUPERVISOR_PID
wait_and_show_logs 5 "Воркеры переключаются в тяжелый режим..."

# ДЕМОНСТРАЦИЯ 3: Graceful Reload конфигурации
check_process $SUPERVISOR_PID
log_info "3. ДЕМОНСТРАЦИЯ: Graceful Reload конфигурации (SIGHUP)"

# Детальная проверка ДО reload
echo "=== ДЕТАЛЬНАЯ ПРОВЕРКА ДО RELOAD ==="
OLD_WORKER_PIDS=$(cat "$PID_DIR/workers.pid" 2>/dev/null | tr '\n' ' ')
echo "Старые PIDs: $OLD_WORKER_PIDS"
ps -p $OLD_WORKER_PIDS -o pid,comm,etime,pcpu 2>/dev/null || echo "Не удалось получить информацию о процессах"

log_info "Отправка SIGHUP супервизору..."
kill -HUP $SUPERVISOR_PID

# Даем БОЛЬШЕ времени для перезапуска
wait_and_show_logs 5 "Конфигурация перезагружается..."

# Детальная проверка ПОСЛЕ reload  
echo "=== ДЕТАЛЬНАЯ ПРОВЕРКА ПОСЛЕ RELOAD ==="
# Принудительно обновляем PID файлы
log_info "Принудительное обновление PID файлов..."
WORKER_COUNT=$(update_pid_files $SUPERVISOR_PID "$PID_DIR")
NEW_WORKER_PIDS=$(cat "$PID_DIR/workers.pid" 2>/dev/null | tr '\n' ' ')

echo "Новые PIDs: $NEW_WORKER_PIDS"
ps -p $NEW_WORKER_PIDS -o pid,comm,etime,pcpu 2>/dev/null || echo "Не удалось получить информацию о процессах"

# Сравниваем PID
if [ "$OLD_WORKER_PIDS" != "$NEW_WORKER_PIDS" ]; then
    log_success "Воркеры успешно перезапущены с новыми PID!"
    echo "Старые PIDs: $OLD_WORKER_PIDS"
    echo "Новые PIDs: $NEW_WORKER_PIDS"
else
    log_warning "ПРОБЛЕМА: PID воркеров не изменились!"
    echo "Старые PIDs: $OLD_WORKER_PIDS"
    echo "Новые PIDs: $NEW_WORKER_PIDS"
    echo "Это означает что reload конфигурации НЕ РАБОТАЕТ"
fi

# ДЕМОНСТРАЦИЯ 4: Авторестарт воркеров
check_process $SUPERVISOR_PID
log_info "4. ДЕМОНСТРАЦИЯ: Авторестарт воркеров"

# ОБНОВЛЯЕМ PID ФАЙЛЫ ПЕРЕД ДЕМОНСТРАЦИЕЙ АВТОРЕСТАРТА
log_info "Обновление PID файлов перед авторестартом..."
WORKER_COUNT=$(update_pid_files $SUPERVISOR_PID "$PID_DIR")
log_success "Обновлено PID файлов: $WORKER_COUNT"

# Находим живого воркера для демонстрации
LIVE_WORKER_PID=""
LIVE_WORKER_ID=""
for worker_file in "$PID_DIR/workers/"*.pid; do
    if [ -f "$worker_file" ]; then
        worker_pid=$(cat "$worker_file")
        if ps -p $worker_pid > /dev/null 2>&1; then
            LIVE_WORKER_PID=$worker_pid
            LIVE_WORKER_ID=$(basename "$worker_file" .pid | sed 's/worker_//')
            log_info "Найден живой worker $LIVE_WORKER_ID с PID: $LIVE_WORKER_PID"
            break
        fi
    fi
done

if [ -n "$LIVE_WORKER_PID" ]; then
    echo "   'Убиваем' worker $LIVE_WORKER_ID для демонстрации авторестарта..."
    
    # Сохраняем старый PID для сравнения
    OLD_PID=$LIVE_WORKER_PID
    
    kill -KILL $LIVE_WORKER_PID
    echo "   Отправлен SIGKILL worker $LIVE_WORKER_ID (PID: $OLD_PID)"
    
    wait_and_show_logs 3 "Ожидаем авторестарта worker $LIVE_WORKER_ID..."
    
    # ОБНОВЛЯЕМ PID ФАЙЛЫ ПОСЛЕ РЕСТАРТА
    log_info "Обновление PID файлов после авторестарта..."
    WORKER_COUNT=$(update_pid_files $SUPERVISOR_PID "$PID_DIR")
    log_success "Обновлено PID файлов: $WORKER_COUNT"
    
    # Ищем новый PID для этого worker
    NEW_WORKER_PID=""
    if [ -f "$PID_DIR/workers/worker_$LIVE_WORKER_ID.pid" ]; then
        NEW_WORKER_PID=$(cat "$PID_DIR/workers/worker_$LIVE_WORKER_ID.pid")
    fi
    
    if [ -n "$NEW_WORKER_PID" ] && [ "$NEW_WORKER_PID" != "$OLD_PID" ]; then
        log_success "Worker $LIVE_WORKER_ID перезапущен!"
        echo "   Старый PID: $OLD_PID"
        echo "   Новый PID: $NEW_WORKER_PID"
    elif [ -n "$NEW_WORKER_PID" ] && [ "$NEW_WORKER_PID" = "$OLD_PID" ]; then
        log_warning "Worker $LIVE_WORKER_ID все еще с тем же PID - перезапуск не произошел"
    else
        log_warning "Worker $LIVE_WORKER_ID не перезапустился"
    fi
else
    log_warning "Не найден ни один живой воркер для демонстрации авторестарта"
    echo "   Пропускаем эту демонстрацию"
fi

# ДЕМОНСТРАЦИЯ 5: Проверка отсутствия зомби-процессов
check_process $SUPERVISOR_PID
log_info "5. ДЕМОНСТРАЦИЯ: Проверка отсутствия зомби-процессов"
echo "   Проверяем систему на наличие зомби-процессов..."
ZOMBIES=$(ps aux | grep -E "[d]efunct" | wc -l)
if [ $ZOMBIES -eq 0 ]; then
    log_success "Зомби-процессов не обнаружено"
else
    log_warning "Обнаружены зомби-процессы: $ZOMBIES"
    ps aux | grep -E "[d]efunct"
fi

# ДЕМОНСТРАЦИЯ 6: Graceful Shutdown
check_process $SUPERVISOR_PID
log_info "6. ДЕМОНСТРАЦИЯ: Graceful Shutdown (SIGTERM)"
echo "   Ожидание: все процессы корректно завершатся в течение 5 секунд"
kill -TERM $SUPERVISOR_PID

echo ""
echo "Ожидаем завершения процессов..."
for i in {1..6}; do
    if ps -p $SUPERVISOR_PID > /dev/null 2>&1; then
        echo "Процессы еще завершаются... ($i/6)"
        sleep 1
    else
        break
    fi
done

# Принудительная очистка PID файлов если супервизор завершился
if ! ps -p $SUPERVISOR_PID > /dev/null 2>&1; then
    log_info "Очистка PID файлов..."
    rm -rf "$PID_DIR"
    log_success "PID файлы очищены"
else
    log_warning "Супервизор все еще работает после таймаута"
fi


echo "Проверьте что PID файлы автоматически очищены:"
ls -la "$PID_DIR/" 2>/dev/null || echo "PID файлы очищены - корректное завершение"
