#!/usr/bin/env bash

CONFIG_FILE='./supervisor.env'
WORKERS=3
MODE_HEAVY_WORK_US=900
MODE_HEAVY_SLEEP_US=100
MODE_LIGHT_WORK_US=200
MODE_LIGHT_SLEEP_US=800

# Базовые настройки для воркеров
CURRENT_MODE="heavy"
SHUTDOWN_REQUESTED=false
RELOAD_REQUESTED=false

# Списки учёта воркеров для супервизора
declare -a WORKER_PIDS
declare -a WORKER_RESTARTS
declare -a WORKER_NICE
declare -a WORKER_CPUS


log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a supervisor.log
}


read_config() {
    if [[ ! -f "$CONFIG_FILE" ]]; then
        log "Config file $CONFIG_FILE not found, using defaults"
        return 1
    fi

    WORKERS=$(grep 'WORKERS' supervisor.env | cut -d '=' -f 2)
    MODE_HEAVY_WORK_US=$(grep 'MODE_HEAVY_WORK_US' supervisor.env | cut -d '=' -f 2)
    MODE_HEAVY_SLEEP_US=$(grep 'MODE_HEAVY_SLEEP_US' supervisor.env | cut -d '=' -f 2)
    MODE_LIGHT_WORK_US=$(grep 'MODE_LIGHT_WORK_US' supervisor.env | cut -d '=' -f 2)
    MODE_LIGHT_SLEEP_US=$(grep 'MODE_LIGHT_SLEEP_US' supervisor.env | cut -d '=' -f 2)

    if [[ -z "$WORKERS" || "$WORKERS" -lt 2 ]]; then
        log "Invalid workers count: $WORKERS, using default: 3"
        WORKERS=3
    fi

    if ! bc --help >/dev/null; then
        echo "bc command not found, please install bc"
        exit 1
    fi
}


setup_scheduling() {
    local worker_id=$1
    local pid=$2
    
    if [[ $worker_id -lt $((WORKERS/2)) ]]; then
        renice -n 10 -p $pid > /dev/null 2>&1
        WORKER_NICE[$worker_id]=10
        log "Worker $worker_id ($pid): set nice=10"
    else
        renice -n 0 -p $pid > /dev/null 2>&1
        WORKER_NICE[$worker_id]=0
        log "Worker $worker_id ($pid): set nice=0"
    fi
    
    local cpu_mask
    if [[ $((worker_id % 2)) -eq 0 ]]; then
        cpu_mask=1  # CPU 0
        WORKER_CPUS[$worker_id]=0
    elif [[ $((worker_id % 2)) -eq 1 ]]; then
        cpu_mask=2  # CPU 1
        WORKER_CPUS[$worker_id]=1
    else
        cpu_mask=3  # CPU 2
        WORKER_CPUS[$worker_id]=2
    fi

    if command -v taskset > /dev/null 2>&1; then
        taskset -p $cpu_mask $pid > /dev/null 2>&1
        log "Worker $worker_id ($pid): set CPU affinity to mask $cpu_mask (CPU $((cpu_mask-1)))"
    else
        log "Warning: taskset not available, CPU affinity not set"
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
        while [[ $(($(date +%s%3N) - start)) -lt $((work_us)) ]]; do
            :
        done
        local work_end=$(date +%s%3N)
        local work_duration=$((work_end - work_start))

        local sleep_start=$(date +%s%3N)
        if [[ $sleep_us -gt 0 ]]; then
            sleep $(echo "scale=3; $sleep_us/1000" | bc) 2>/dev/null || sleep 0.001
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
        local worker_pid=$!
        WORKER_PIDS[$i]=$worker_pid
        WORKER_RESTARTS[$i]=""

        sleep 0.1
        setup_scheduling $i $worker_pid

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
                local new_pid=$!
                WORKER_PIDS[$i]=$new_pid
                WORKER_RESTARTS[$i]="${recent_restarts[*]} $now"
                
                sleep 0.1
                setup_scheduling $i $new_pid
                
                log "Restarted worker $i with PID ${WORKER_PIDS[$i]}"
            else
                log "Restart limit exceeded for worker $i, waiting..."
                WORKER_RESTARTS[$i]="${recent_restarts[*]}"
            fi
        fi
    done
}


show_worker_stats() {
    log "=== Worker Scheduling Statistics ==="
    for i in "${!WORKER_PIDS[@]}"; do
        local pid=${WORKER_PIDS[$i]}
        if kill -0 "$pid" 2>/dev/null; then
            local nice_value=${WORKER_NICE[$i]:-"unknown"}
            local cpu_value=${WORKER_CPUS[$i]:-"unknown"}
            log "Worker $i (PID $pid): nice=$nice_value, CPU=$cpu_value"
        fi
    done
    log "==================================="
}


# Главная функция
main() {
    log "Supervisor started with PID $BASHPID"

    trap 'log "Get SIGTERM/SIGINT"; SHUTDOWN_REQUESTED=true' SIGTERM SIGINT
    trap 'log "Get EXIT"; SHUTDOWN_REQUESTED=true' EXIT
    trap 'log "Get SIGHUP"; RELOAD_REQUESTED=true' SIGHUP
    trap 'log "Get SIGUSR1"; CURRENT_MODE="light"; for pid in "${WORKER_PIDS[@]}"; do kill -SIGUSR1 "$pid" 2>/dev/null; done' SIGUSR1
    trap 'log "Get SIGUSR2"; CURRENT_MODE="heavy"; for pid in "${WORKER_PIDS[@]}"; do kill -SIGUSR2 "$pid" 2>/dev/null; done' SIGUSR2
    trap 'show_worker_stats' SIGTRAP
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
