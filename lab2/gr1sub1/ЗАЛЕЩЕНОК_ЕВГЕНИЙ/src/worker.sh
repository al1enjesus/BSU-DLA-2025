#!/bin/bash

# Изначально режим "тяжелый"
MODE="heavy"
WORK_US=$WORK_HEAVY_US
SLEEP_US=$SLEEP_HEAVY_US
RUNNING=true

# Функция для вывода логов
log() {
    echo "[WORKER $$] [CPU $(taskset -p $$ | awk '{print $NF}')] $1"
}

# Функция-обработчик сигналов
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

# Устанавливаем ловушки на сигналы
trap 'handle_signal TERM' SIGTERM
trap 'handle_signal USR1' SIGUSR1
trap 'handle_signal USR2' SIGUSR2

log "Запущен. Режим: $MODE"

# Основной цикл работы
while $RUNNING; do
    log "Работаю ($MODE)..."
    # Имитация вычислений (нагрузка на CPU)
    for ((i=0; i<WORK_US; i++)); do :; done
    # Имитация ожидания
    sleep $(echo "$SLEEP_US / 1000000" | bc -l)
done

log "Работа завершена."
exit 0
