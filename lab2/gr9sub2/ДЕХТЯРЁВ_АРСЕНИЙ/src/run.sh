#!/bin/bash

set -e

SCRIPT_PATH="."
CONFIG_FILE="supervisor.env"
SUPERVISOR_SCRIPT="supervisor.sh"
SUPERVISOR_PID=""

# Функции для управления
start_supervisor() {
    echo "=== Starting Supervisor ==="
    if [[ ! -f "$SCRIPT_PATH/$CONFIG_FILE" ]]; then
        echo "Creating default config..."
        cat > "$SCRIPT_PATH/$CONFIG_FILE" << EOF
WORKERS=4
MODE_HEAVY_WORK_US=9000
MODE_HEAVY_SLEEP_US=1000
MODE_LIGHT_WORK_US=2000
MODE_LIGHT_SLEEP_US=8000
EOF
    fi
    
    chmod +x "$SCRIPT_PATH/$SUPERVISOR_SCRIPT"
    cd $SCRIPT_PATH
    ./"$SUPERVISOR_SCRIPT" &
    SUPERVISOR_PID=$!
    cd -
    echo "Supervisor started with PID: $SUPERVISOR_PID"
    sleep 2
}

stop_supervisor() {
    echo "=== Stopping Supervisor ==="
    pkill -P $$
    echo "=== Supervisor stopped ==="
}

show_processes() {
    echo "=== Current Processes ==="
    ps -eo pid,ni,psr,pcpu,comm | grep -E "(supervisor|worker)" | grep -v grep || true
}

show_detailed_stats() {
    echo "=== Detailed Worker Statistics ==="
    if [[ -n "$SUPERVISOR_PID" ]] && kill -0 "$SUPERVISOR_PID" 2>/dev/null; then
        kill -USR1 "$SUPERVISOR_PID"
        sleep 1
    fi
    echo "=== Process details ==="
    ps -eo pid,ni,psr,pcpu,comm | grep -E "(supervisor|worker)" | grep -v grep || true
}

# Демонстрационные сценарии
demo_graceful_shutdown() {
    echo "### Demo: Graceful Shutdown ###"
    start_supervisor
    sleep 3
    echo "Sending SIGTERM to supervisor..."
    kill -TERM "$SUPERVISOR_PID"
    sleep 6
    show_processes
}

demo_reload() {
    echo "### Demo: Config Reload ###"
    start_supervisor
    sleep 3
    echo "Current workers: 4"
    show_processes
    
    echo "Changing config to 6 workers..."
    sed -i 's/WORKERS=4/WORKERS=6/' "$SCRIPT_PATH/$CONFIG_FILE"
    echo "Sending SIGHUP for reload..."
    kill -HUP "$SUPERVISOR_PID"
    sleep 3
    show_processes
    
    stop_supervisor
}

demo_mode_switch() {
    echo "### Demo: Mode Switching ###"
    start_supervisor
    sleep 2
    
    echo "Switching to light mode (SIGUSR1)..."
    kill -USR1 "$SUPERVISOR_PID"
    sleep 3
    
    echo "Switching to heavy mode (SIGUSR2)..."
    kill -USR2 "$SUPERVISOR_PID"
    sleep 3
    
    stop_supervisor
}

demo_restart_policy() {
    echo "### Demo: Restart Policy ###"
    start_supervisor
    sleep 2
    
    echo "Killing one worker to test restart..."
    WORKER_PIDS=$(ps aux | grep "worker" | grep -v grep | awk '{print $2}' | head -1)
    for pid in $WORKER_PIDS; do
        echo "Killing worker PID: $pid"
        kill -KILL "$pid"
    done
    
    sleep 2
    echo "Checking if worker was restarted..."
    show_processes
    
    echo "Rapidly killing workers to test rate limiting..."
    for i in {1..3}; do
        WORKER_PIDS=$(ps aux | grep "worker" | grep -v grep | awk '{print $2}' | head -1)
        for pid in $WORKER_PIDS; do
            kill -KILL "$pid"
        done
        sleep 1
    done
    
    sleep 2
    show_processes
    stop_supervisor
}

demo_nice_effect() {
    echo "### Demo: Nice Value Effect ###"
    start_supervisor
    sleep 2
    
    echo "=== Initial worker scheduling setup ==="
    show_detailed_stats
    
    echo "=== Monitoring CPU distribution for 10 seconds ==="
    echo "Workers with nice=10 should get less CPU time than workers with nice=0"
    echo "Starting pidstat monitoring..."
    
    # Собираем PID всех worker процессов
    WORKER_PIDS=$(ps aux | grep "worker" | grep -v grep | awk '{print $2}' | tr '\n' ',' | sed 's/,$//')
    
    if [[ -n "$WORKER_PIDS" ]]; then
        # Запускаем pidstat в фоне
        pidstat -u -p "$WORKER_PIDS" 1 10 > pidstat_nice.log &
        PIDSTAT_PID=$!
        
        echo "pidstat running with PID: $PIDSTAT_PID, logging to pidstat_nice.log"
        sleep 11
        
        # Показываем результаты
        echo "=== pidstat Results ==="
        grep -A 20 "Average:" pidstat_nice.log || cat pidstat_nice.log
        
        echo "=== Analysis ==="
        echo "Workers with nice=10 (first half): expected ~20-25% CPU"
        echo "Workers with nice=0 (second half): expected ~80-90% CPU"
    else
        echo "No worker PIDs found for monitoring"
    fi
    
    stop_supervisor
}

demo_affinity_effect() {
    echo "### Demo: CPU Affinity Effect ###"
    start_supervisor
    sleep 2
    
    echo "=== Worker CPU affinity setup ==="
    show_detailed_stats
    
    echo "=== Monitoring CPU core distribution ==="
    echo "Workers should be pinned to specific CPU cores:"
    echo "- First quarter: CPU 0"
    echo "- Second quarter: CPU 1" 
    echo "- Third quarter: CPU 0"
    echo "- Fourth quarter: CPU 1"
    
    # Мониторим распределение по ядрам
    echo "=== Running core distribution monitoring ==="
    for i in {1..5}; do
        echo "--- Sample $i ---"
        ps -eo pid,psr,comm | grep worker | grep -v grep | sort -n
        sleep 2
    done
    
    echo "=== CPU affinity verification ==="
    WORKER_PIDS=$(ps aux | grep "worker" | grep -v grep | awk '{print $2}')
    for pid in $WORKER_PIDS; do
        if command -v taskset > /dev/null 2>&1; then
            affinity=$(taskset -p $pid 2>/dev/null | awk '{print $6}' || echo "N/A")
            echo "PID $pid: CPU affinity mask = $affinity"
        else
            psr=$(ps -o psr -p $pid 2>/dev/null | tail -1 | tr -d ' ')
            echo "PID $pid: Running on CPU $psr"
        fi
    done
    
    stop_supervisor
}

demo_combined_scheduling() {
    echo "### Demo: Combined Nice + Affinity ###"
    start_supervisor
    sleep 2
    
    echo "=== Complete scheduling setup ==="
    show_detailed_stats
    
    echo "=== Detailed process information ==="
    echo "PID | NI | PSR | %CPU | COMMAND"
    ps -eo pid,ni,psr,pcpu,comm | grep -E "(supervisor|worker)" | grep -v grep | sort -k2,2n -k3,3n
    
    echo ""
    echo "=== Legend ==="
    echo "NI (Nice): 10 = low priority, 0 = normal priority"
    echo "PSR (Processor): CPU core the process is running on"
    echo "%CPU: CPU usage percentage"
    
    echo ""
    echo "=== Expected distribution ==="
    echo "Workers 0-1: nice=10, CPU 0-1"
    echo "Workers 2-3: nice=0, CPU 0-1"
    
    # Запускаем мониторинг производительности
    echo ""
    echo "=== Starting performance monitoring ==="
    WORKER_PIDS=$(ps aux | grep "worker" | grep -v grep | awk '{print $2}' | tr '\n' ',' | sed 's/,$//')
    
    if [[ -n "$WORKER_PIDS" ]]; then
        timeout 10s pidstat -u -p "$WORKER_PIDS" 1 > pidstat_combined.log 2>&1 &
        echo "Monitoring for 10 seconds..."
        sleep 11
        
        echo "=== Performance Results ==="
        if [[ -f "pidstat_combined.log" ]]; then
            grep -E "(CPU|worker|Average|^[0-9])" pidstat_combined.log | head -30
        fi
    fi
    
    stop_supervisor
}

trap 'pkill -P $$; rm -f pidstat_nice.log pidstat_combined.log' EXIT

if [ $# -lt 1 ];then
    echo "Usage: $0 {shutdown|reload|mode|restart|nice|affinity|scheduling|all}"
    echo "  shutdown   - Demo graceful shutdown"
    echo "  reload     - Demo config reload" 
    echo "  mode       - Demo mode switching"
    echo "  restart    - Demo restart policy"
    echo "  nice       - Demo nice value effect"
    echo "  affinity   - Demo CPU affinity effect"
    echo "  scheduling - Demo combined nice + affinity"
    echo "  all        - Run all demos"
    exit 1
fi

# Основное меню
case "${1:-all}" in
    shutdown)
        demo_graceful_shutdown
        ;;
    reload)
        demo_reload
        ;;
    mode)
        demo_mode_switch
        ;;
    restart)
        demo_restart_policy
        ;;
    nice)
        demo_nice_effect
        ;;
    affinity)
        demo_affinity_effect
        ;;
    scheduling)
        demo_combined_scheduling
        ;;
    all)
        echo "=== Running All Demos ==="
        demo_graceful_shutdown
        echo
        demo_reload  
        echo
        demo_mode_switch
        echo
        demo_restart_policy
        echo
        demo_nice_effect
        echo
        demo_affinity_effect
        echo
        demo_combined_scheduling
        ;;
    *)
        echo "Usage: $0 {shutdown|reload|mode|restart|nice|affinity|scheduling|all}"
        echo "  shutdown   - Demo graceful shutdown"
        echo "  reload     - Demo config reload" 
        echo "  mode       - Demo mode switching"
        echo "  restart    - Demo restart policy"
        echo "  nice       - Demo nice value effect"
        echo "  affinity   - Demo CPU affinity effect"
        echo "  scheduling - Demo combined nice + affinity"
        echo "  all        - Run all demos"
        exit 1
        ;;
esac

echo "=== Demo Completed ==="

rm -f pidstat_nice.log pidstat_combined.log
