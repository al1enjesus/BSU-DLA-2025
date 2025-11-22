#!/bin/bash

CONFIG_FILE="src/config.conf"
WORKER_SCRIPT="src/worker.sh"

check_utils() {
    for util in "$@"; do
        if ! command -v "$util" &> /dev/null; then
            echo "Ошибка: Утилита '$util' не найдена. Пожалуйста, установите ее." >&2
            exit 1
        fi
    done
}

check_utils bash kill sleep wait

if [ -f "$CONFIG_FILE" ]; then
    source "$CONFIG_FILE"
else
    echo "Ошибка: Конфигурационный файл $CONFIG_FILE не найден."
    exit 1
fi

declare -a WORKER_PIDS
RUNNING=true

log() {
    echo "[SUPERVISOR $$] $1"
}

spawn_workers() {
    log "Запуск $workers воркеров..."
    for ((i=0; i<workers; i++)); do
        export WORK_HEAVY_US=$work_heavy_us
        export SLEEP_HEAVY_US=$sleep_heavy_us
        export WORK_LIGHT_US=$work_light_us
        export SLEEP_LIGHT_US=$sleep_light_us

        bash "$WORKER_SCRIPT" &
        WORKER_PIDS+=($!)
        log "Воркер запущен с PID: ${WORKER_PIDS[-1]}"
    done
}

graceful_shutdown() {
    log "Получен сигнал завершения. Отправка SIGTERM воркерам..."
    RUNNING=false
    if [ ${#WORKER_PIDS[@]} -gt 0 ]; then
        kill -SIGTERM "${WORKER_PIDS[@]}" 2>/dev/null
    fi
    
    sleep 5
    
    log "Проверка оставшихся воркеров..."
    for pid in "${WORKER_PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            log "Воркер $pid не завершился. Отправка SIGKILL."
            kill -SIGKILL "$pid" 2>/dev/null
        fi
    done

    log "Супервизор завершает работу."
    exit 0
}

graceful_reload() {
    log "Получен SIGHUP. 'Мягкий' перезапуск воркеров..."
    local old_pids=("${WORKER_PIDS[@]}")
    WORKER_PIDS=()
    
    log "Запуск новых воркеров..."
    spawn_workers
    
    log "Отправка SIGTERM старым воркерам: ${old_pids[*]}"
    if [ ${#old_pids[@]} -gt 0 ]; then
        kill -SIGTERM "${old_pids[@]}" 2>/dev/null
        
        sleep 5 
        
        for pid in "${old_pids[@]}"; do
            if kill -0 "$pid" 2>/dev/null; then
                log "Воркер $pid не завершился вовремя. Отправка SIGKILL."
                kill -SIGKILL "$pid" 2>/dev/null
            fi
        done
    fi
    log "Перезапуск завершен."
}

trap 'graceful_shutdown' SIGINT SIGTERM
trap 'graceful_reload' SIGHUP
trap 'log "Получен SIGUSR1"; if [ ${#WORKER_PIDS[@]} -gt 0 ]; then kill -SIGUSR1 "${WORKER_PIDS[@]}"; fi' SIGUSR1
trap 'log "Получен SIGUSR2"; if [ ${#WORKER_PIDS[@]} -gt 0 ]; then kill -SIGUSR2 "${WORKER_PIDS[@]}"; fi' SIGUSR2

log "Супервизор запущен."
spawn_workers

while $RUNNING; do
    wait -n 2>/dev/null
    EXIT_CODE=$?
    
    if [ $EXIT_CODE -gt 128 ]; then continue; fi

    for i in "${!WORKER_PIDS[@]}"; do
        pid=${WORKER_PIDS[$i]}
        if ! kill -0 "$pid" 2>/dev/null; then
            log "Воркер $pid завершился с кодом $EXIT_CODE."
            unset 'WORKER_PIDS[i]'
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
