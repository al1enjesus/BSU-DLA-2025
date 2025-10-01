#!/usr/bin/env bash

CONFIG_FILE='./supervisor.env'
WORKERS=3
MODE_HEAVY_WORK_US=900000
MODE_HEAVY_SLEEP_US=100000
MODE_LIGHT_WORK_US=200000
MODE_LIGHT_SLEEP_US=800000

# Базовые настройки для воркеров
CURRENT_MODE="heavy"
SHUTDOWN_REQUESTED=false
RELOAD_REQUESTED=false

# Списки учёта воркеров для супервизора
declare -a WORKER_PIDS
declare -a WORKER_RESTARTS


log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a supervisor.log
}


read_config() {
    if [[ ! -f "$CONFIG_FILE" ]]; then
        log "Config file $CONFIG_FILE not found, using defaults"
        return 1
    fi
    
    source "$CONFIG_FILE"
    
    if [[ -z "$WORKERS" || "$WORKERS" -lt 2 ]]; then
        log "Invalid workers count: $WORKERS, using default: 3"
        WORKERS=3
    fi
}


# Функция воркера
worker(){
    local worker_id=$1
    local mode=$CURRENT_MODE

    trap 'echo "Worker $worker_id ($BASHPID): Received SIGTERM, shutting down"; exit 0' SIGTERM SIGINT SIGKILL
    trap 'mode="light"; echo "Worker $worker_id ($BASHPID): Switched to light mode"' SIGUSR1
    trap 'mode="heavy"; echo "Worker $worker_id ($BASHPID): Switched to heavy mode"' SIGUSR2

    local iteration=0
    local total_work_time=0
    local total_sleep_time=0

    while true; do
        local work_us sleep_us
        if [[ "$mode" == "heavy" ]]; then
            work_us=$MODE_HEAVY_WORK_US
            sleep_us=$MODE_HEAVY_SLEEP_US
        else
            work_us=$MODE_LIGHT_WORK_US
            sleep_us=$MODE_LIGHT_SLEEP_US
        fi

        local work_start=$(date +%s%3N)
        local start=$(date +%s%3N)
        while [[ $(($(date +%s%3N) - start)) -lt $((work_us/1000)) ]]; do
            :
        done
        local work_end=$(date +%s%3N)
        local work_duration=$((work_end - work_start))

        local sleep_start=$(date +%s%3N)
        local sleep_ms=$((sleep_us/1000))
        if [[ $sleep_ms -gt 0 ]]; then
            sleep $(echo "scale=3; $sleep_ms/1000" | bc) 2>/dev/null || sleep 0.001
        fi
        local sleep_end=$(date +%s%3N)
        local sleep_duration=$((sleep_end - sleep_start))

        iteration=$((iteration + 1))
        total_work_time=$((total_work_time + work_duration))
        total_sleep_time=$((total_sleep_time + sleep_duration))

        local cpu_info="unknown"
        if [[ -f "/proc/self/stat" ]]; then
            cpu_info=$(cat /proc/self/stat | awk '{print $39}')
        fi

        log "Worker $worker_id ($BASHPID): mode=$mode, iter=$iteration, work=${work_duration}ms, sleep=${sleep_duration}ms, cpu=$cpu_info, avg_work=$((total_work_time/iteration))ms, avg_sleep=$((total_sleep_time/iteration))ms"
    done
}


# Управление с воркерами
start_workers() {
    log "Starting $WORKERS workers in $CURRENT_MODE mode"
    for ((i=0; i<WORKERS; i++)); do
        worker $i &
        WORKER_PIDS[$i]=$!
        WORKER_RESTARTS[$i]=""
        log "Started worker $i with PID ${WORKER_PIDS[$i]}"
    done
}

stop_workers() {
    log "Stopping workers..."
    for pid in "${WORKER_PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            kill -SIGTERM "$pid" 2>/dev/null
        fi
    done
    
    local timeout=5
    local start_time=$(date +%s)
    
    for pid in "${WORKER_PIDS[@]}"; do
        local waited=0
        while kill -0 "$pid" 2>/dev/null && [[ $waited -lt $timeout ]]; do
            sleep 0.1
            waited=$(( $(date +%s) - start_time ))
        done
        
        if kill -0 "$pid" 2>/dev/null; then
            log "Force killing worker $pid"
            kill -SIGKILL "$pid" 2>/dev/null
        fi
    done
    
    WORKER_PIDS=()
    log "All workers stopped"
}

restart_workers() {
    stop_workers
    start_workers
}

check_workers() {
    if $SHUTDOWN_REQUESTED; then
        echo "Shutdown requested, skip check"
        return 0
    fi

    for i in "${!WORKER_PIDS[@]}"; do
        local pid=${WORKER_PIDS[$i]}
        
        if ! kill -0 "$pid" 2>/dev/null; then
            log "Worker $i (PID $pid) died, checking restart policy..."
            
            local now=$(date +%s)
            local restarts=(${WORKER_RESTARTS[$i]})
            local recent_restarts=()
            
            for restart in "${restarts[@]}"; do
                if [[ -n "$restart" && $((now - restart)) -le 30 ]]; then
                    recent_restarts+=("$restart")
                fi
            done
            
            if [[ ${#recent_restarts[@]} -lt 5 ]]; then
                log "Restarting worker $i..."
                worker $i &
                WORKER_PIDS[$i]=$!
                WORKER_RESTARTS[$i]="${recent_restarts[*]} $now"
                log "Restarted worker $i with PID ${WORKER_PIDS[$i]}"
            else
                log "Restart limit exceeded for worker $i, waiting..."
                WORKER_RESTARTS[$i]="${recent_restarts[*]}"
            fi
        fi
    done
}


# Главная функция
main() {
    log "Supervisor started with PID $BASHPID"

    trap 'log "Get SIGTERM/SIGINT"; SHUTDOWN_REQUESTED=true' SIGTERM SIGINT
    trap 'log "Get EXIT"; SHUTDOWN_REQUESTED=true' EXIT
    trap 'log "Get SIGHUP"; RELOAD_REQUESTED=true' SIGHUP
    trap 'log "Get SIGUSR1"; CURRENT_MODE="light"; for pid in "${WORKER_PIDS[@]}"; do kill -SIGUSR1 "$pid" 2>/dev/null; done' SIGUSR1
    trap 'log "Get SIGUSR2"; CURRENT_MODE="heavy"; for pid in "${WORKER_PIDS[@]}"; do kill -SIGUSR2 "$pid" 2>/dev/null; done' SIGUSR2
    trap 'check_workers' SIGCHLD

    read_config

    start_workers

    while true; do

        check_workers

        if $SHUTDOWN_REQUESTED; then
            stop_workers
            log "Supervisor shutdown completed"
            exit 0
        fi

        if $RELOAD_REQUESTED; then
            log "Reloading configuration..."
            read_config
            restart_workers
            RELOAD_REQUESTED=false
        fi

        sleep 0.1
    done
}

main
