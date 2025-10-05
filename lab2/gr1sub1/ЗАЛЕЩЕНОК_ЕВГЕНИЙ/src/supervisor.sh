#!/bin/bash

# Путь к файлам
CONFIG_FILE="src/config.conf"
WORKER_SCRIPT="src/worker.sh"

# Загрузка конфигурации
if [ -f "$CONFIG_FILE" ]; then
    source "$CONFIG_FILE"
else
    echo "Ошибка: Конфигурационный файл $CONFIG_FILE не найден."
    exit 1
fi

# Глобальные переменные
declare -a WORKER_PIDS
RUNNING=true

log() {
    echo "[SUPERVISOR $$] $1"
}

# Запуск N воркеров
spawn_workers() {
    log "Запуск $workers воркеров..."
    for ((i=0; i<workers; i++)); do
        # Экспортируем переменные для воркера
        export WORK_HEAVY_US=$work_heavy_us
        export SLEEP_HEAVY_US=$sleep_heavy_us
        export WORK_LIGHT_US=$work_light_us
        export SLEEP_LIGHT_US=$sleep_light_us

        bash "$WORKER_SCRIPT" &
        WORKER_PIDS+=($!)
        log "Воркер запущен с PID: ${WORKER_PIDS[-1]}"
    done
}

# Функция корректного завершения
graceful_shutdown() {
    log "Получен сигнал завершения. Отправка SIGTERM воркерам..."
    RUNNING=false
    # Отправляем SIGTERM всем дочерним процессам
    kill -SIGTERM "${WORKER_PIDS[@]}"
    
    # Ждем завершения в течение 5 секунд
    sleep 5
    
    log "Проверка оставшихся воркеров..."
    for pid in "${WORKER_PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            log "Воркер $pid не завершился. Отправка SIGKILL."
            kill -SIGKILL "$pid"
        fi
    done

    log "Супервизор завершает работу."
    exit 0
}

# Функция "мягкой" перезагрузки
graceful_reload() {
    log "Получен SIGHUP. Перезапуск воркеров..."
    # Сначала останавливаем старых
    kill -SIGTERM "${WORKER_PIDS[@]}"
    wait "${WORKER_PIDS[@]}" 2>/dev/null
    WORKER_PIDS=()
    # Затем запускаем новых
    spawn_workers
}

# Обработка сигналов
trap 'graceful_shutdown' SIGINT SIGTERM
trap 'graceful_reload' SIGHUP
trap 'log "Получен SIGUSR1"; kill -SIGUSR1 "${WORKER_PIDS[@]}"' SIGUSR1
trap 'log "Получен SIGUSR2"; kill -SIGUSR2 "${WORKER_PIDS[@]}"' SIGUSR2

# --- Основная логика ---
log "Супервизор запущен."
spawn_workers

# Основной цикл - ожидание и проверка дочерних процессов
while $RUNNING; do
    # Ждем завершения любого дочернего процесса
    wait -n
    EXIT_CODE=$?
    # Находим, какой воркер завершился
    for i in "${!WORKER_PIDS[@]}"; do
        pid=${WORKER_PIDS[$i]}
        if ! kill -0 "$pid" 2>/dev/null; then
            log "Воркер $pid завершился с кодом $EXIT_CODE."
            # Удаляем его из списка
            unset 'WORKER_PIDS[i]'
            # Перезапускаем, если супервизор еще работает
            if $RUNNING; then
                log "Перезапуск воркера..."
                bash "$WORKER_SCRIPT" &
                WORKER_PIDS+=($!)
                log "Новый воркер запущен с PID: ${WORKER_PIDS[-1]}"
            fi
            break
        fi
    done
done
