#!/bin/bash

MODE="heavy"
WORK_US=$WORK_HEAVY_US
SLEEP_US=$SLEEP_HEAVY_US
RUNNING=true

log() {
    local cpu_info
    if command -v taskset &> /dev/null; then
        cpu_info="[CPU $(taskset -p $$ | awk '{print $NF}')]"
    else
        cpu_info=""
    fi
    echo "[WORKER $$] $cpu_info $1"
}

handle_signal() {
    case "$1" in
        "TERM")
            log "Получен SIGTERM, завершаю работу..."
            RUNNING=false
            ;;
        "USR1")
            log "Переключение в 'легкий' режим (light)"
            MODE="light"
            WORK_US=$WORK_LIGHT_US
            SLEEP_US=$SLEEP_LIGHT_US
            ;;
        "USR2")
            log "Переключение в 'тяжелый' режим (heavy)"
            MODE="heavy"
            WORK_US=$WORK_HEAVY_US
            SLEEP_US=$SLEEP_HEAVY_US
            ;;
    esac
}

trap 'handle_signal TERM' SIGTERM
trap 'handle_signal USR1' SIGUSR1
trap 'handle_signal USR2' SIGUSR2

log "Запущен. Режим: $MODE"

while $RUNNING; do
    log "Работаю ($MODE)..."
    for ((i=0; i<WORK_US; i++)); do :; done
    
    seconds=$((SLEEP_US / 1000000))
    microseconds=$((SLEEP_US % 1000000))
    sleep "$seconds.$microseconds"
done

log "Работа завершена."
exit 0
