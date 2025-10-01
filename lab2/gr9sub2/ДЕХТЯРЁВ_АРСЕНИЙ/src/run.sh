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
WORKERS=3
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
    ps aux | grep -E "(supervisor|worker)" | grep -v grep || true
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
    echo "Current workers: 3"
    show_processes
    
    echo "Changing config to 4 workers..."
    sed -i 's/workers=3/workers=4/' "$SCRIPT_PATH/$CONFIG_FILE"
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

trap 'pkill -P $$' EXIT

if [ $# -lt 1 ];then
    echo "Usage: $0 {shutdown|reload|mode|restart|all}"
    echo "  shutdown - Demo graceful shutdown"
    echo "  reload   - Demo config reload" 
    echo "  mode     - Demo mode switching"
    echo "  restart  - Demo restart policy"
    echo "  all      - Run all demos"
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
    all)
        echo "=== Running All Demos ==="
        demo_graceful_shutdown
        echo
        demo_reload  
        echo
        demo_mode_switch
        echo
        demo_restart_policy
        ;;
    *)
        echo "Usage: $0 {shutdown|reload|mode|restart|all}"
        echo "  shutdown - Demo graceful shutdown"
        echo "  reload   - Demo config reload" 
        echo "  mode     - Demo mode switching"
        echo "  restart  - Demo restart policy"
        echo "  all      - Run all demos"
        exit 1
        ;;
esac

echo "=== Demo Completed ==="