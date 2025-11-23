#!/usr/bin/env bash
# demo.sh
# Демонстрация работы libsyscall_spy.so с curl и wget.
# Гарантирует: ВСЁ (stdout+stderr + вывод LD_PRELOAD) попадёт в target лог (и в консоль).

set -euo pipefail

LIB="./libsyscall_spy.so"
MAKE_TARGET="all"
LOGS_DIR_A="../../logs/A"

DEFAULT_URLS=(
  "https://example.com/"
  "https://example.org/"
  "https://www.wikipedia.org/"
  "https://www.kernel.org/"
  "https://www.github.com/"
  "https://www.stackoverflow.com/"
  "https://www.google.com/"
  "https://www.yandex.com/"
  "https://nginx.org/"
)

show_help() {
    cat << EOF
Usage: $0 [OPTIONS]

libsyscall_spy.so demonstration curl и wget.

OPTIONS:
    -t, --target TARGET      Target util: curl, wget or all (default: all)
    -u, --urls URLS          URLs, delimeter = ','
    -h, --help               Show this message
EOF
}

count_syscalls() {
    local output="$1"
    local open_count read_count write_count close_count recv_count send_count

    open_count=$(echo "$output" | grep -c "\[SPY\] open" || true)
    open_count=$((open_count + $(echo "$output" | grep -c "\[SPY\] openat" || true)))
    read_count=$(echo "$output" | grep -c "\[SPY\] read" || true)
    write_count=$(echo "$output" | grep -c "\[SPY\] write" || true)
    close_count=$(echo "$output" | grep -c "\[SPY\] close" || true)
    recv_count=$(echo "$output" | grep -c "\[SPY\] recv" || true)
    send_count=$(echo "$output" | grep -c "\[SPY\] send" || true)

    echo "$open_count $read_count $write_count $close_count $recv_count $send_count"
}

print_statistics() {
    local output="$1"
    local target="$2"

    echo ""
    echo "=== Captured Calls Statistics ==="
    echo "Target: $target"
    echo ""

    IFS=' ' read -r open_count read_count write_count close_count recv_count send_count <<< "$(count_syscalls "$output")"

    printf "%-20s | %-10s\n" "System call" "Count"
    printf "%-20s-+-%-10s\n" "--------------------" "----------"
    printf "%-20s | %-10d\n" "open/openat" "$open_count"
    printf "%-20s | %-10d\n" "read" "$read_count"
    printf "%-20s | %-10d\n" "write" "$write_count"
    printf "%-20s | %-10d\n" "close" "$close_count"
    printf "%-20s | %-10d\n" "recv" "$recv_count"
    printf "%-20s | %-10d\n" "send" "$send_count"
    printf "%-20s-+-%-10s\n" "--------------------" "----------"
    local total=$((open_count + read_count + write_count + close_count + recv_count + send_count))
    printf "%-20s | %-10d\n" "TOTAL" "$total"
    echo ""
}

# ---------------------------
# Парсинг аргументов
# ---------------------------
TARGET="all"
URLS=("${DEFAULT_URLS[@]}")

while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--target)
            TARGET="$2"; shift 2 ;;
        -u|--urls)
            IFS=',' read -r -a URLS <<< "$2"; shift 2 ;;
        -h|--help)
            show_help; exit 0 ;;
        *)
            echo "Unknown option: $1"; show_help; exit 1 ;;
    esac
done

case "$TARGET" in
    curl|wget|all) ;;
    *)
        echo "Error: Invalid target '$TARGET'"; exit 1 ;;
esac

if [ ${#URLS[@]} -eq 0 ]; then
    echo "Error: URL list cannot be empty"
    exit 1
fi

# ---------------------------
# Подготовка логов и выбор полного лог-файла (FULL_LOG)
# ---------------------------
mkdir -p "$LOGS_DIR_A"


case "$TARGET" in
    curl)
        CUR_LOG="$LOGS_DIR_A/logs_curl.txt"
        WGET_LOG="/dev/null"
        SPY_RAW="$LOGS_DIR_A/logs_syscalls_raw.txt"
        SPY_SAFE="$LOGS_DIR_A/logs_curl_safe.txt"
        FULL_LOG="$CUR_LOG"            # весь вывод попадёт в CUR_LOG
        ;;
    wget)
        CUR_LOG="/dev/null"
        WGET_LOG="$LOGS_DIR_A/logs_wget.txt"
        SPY_RAW="$LOGS_DIR_A/logs_syscalls_raw.txt"
        SPY_SAFE="$LOGS_DIR_A/logs_wget_safe.txt"
        FULL_LOG="$WGET_LOG"           # весь вывод попадёт в WGET_LOG
        ;;
    all)
        CUR_LOG="$LOGS_DIR_A/logs_curl.txt"
        WGET_LOG="$LOGS_DIR_A/logs_wget.txt"
        SPY_RAW="$LOGS_DIR_A/logs_syscalls_raw.txt"
        SPY_SAFE="$LOGS_DIR_A/logs_syscalls_safe.txt"
        FULL_LOG="$SPY_RAW"            # весь вывод попадёт в единый SPY_RAW
        ;;
esac

# Очистка/создание файлов
: > "$FULL_LOG"
if [ "$CUR_LOG" != "/dev/null" ]; then : > "$CUR_LOG"; fi
if [ "$WGET_LOG" != "/dev/null" ]; then : > "$WGET_LOG"; fi

# Сохраним старый LD_PRELOAD и поднимем trap для восстановления
OLD_LD_PRELOAD="${LD_PRELOAD-}"
restore_ld_preload() {
    if [ -z "$OLD_LD_PRELOAD" ]; then
        unset LD_PRELOAD
    else
        export LD_PRELOAD="$OLD_LD_PRELOAD"
    fi
    echo "[demo] LD_PRELOAD restored to: ${OLD_LD_PRELOAD:-<empty>}"
}
trap restore_ld_preload EXIT INT TERM

# ---------------------------
# ГЛОБАЛЬНЫЙ ЗАХВАТ ВСЕГО ВЫВОДА
# ---------------------------
# ВАЖНО: exec делает так, что ВСЁ (stdout+stderr) текущего процесса и всех дочерних
# (включая LD_PRELOAD библиотеку) попадёт в FULL_LOG и одновременно выводится на консоль.
exec > >(tee -a "$FULL_LOG") 2>&1

echo "[demo] Building library..."
make "$MAKE_TARGET" >/dev/null

if [ ! -f "$LIB" ]; then
    echo "[demo] Error: $LIB not found."
    exit 1
fi

echo "[demo] Setting LD_PRELOAD -> $LIB"
export LD_PRELOAD="$PWD/$LIB"

echo "[demo] Target: $TARGET"
echo "[demo] URLs: ${URLS[*]}"
echo "[demo] Full log (capturing stdout+stderr) -> $FULL_LOG"

# ---------------------------
# Вспомогательная функция выполнения команд
# ---------------------------
# run_and_capture сохраняет вывод каждого вызова в per-utility лог (CUR_LOG/WGET_LOG),
# но основной «full» лог уже ведётся через exec -> FULL_LOG.
run_and_capture() {
    local command="$1"
    local label="$2"
    local output_log="$3"

    echo "[demo] Executing: $command"   # попадёт и в консоль, и в FULL_LOG

    # временный вывод per-command (удобно для отдельных логов)
    local temp_file
    temp_file=$(mktemp)

    # Запуск команды: stdout+stderr дочерних процессов уже перенаправлены глобально в FULL_LOG,
    # но сохраняем также отдельный файл для per-utility логики (чтобы не ломать исходную структуру).
    # Используем обычный запуск — вывод при этом также попадает в FULL_LOG через exec.
    eval "$command" 2>&1 | tee -a "$temp_file"

    echo "=== $label ===" >> "$output_log"
    cat "$temp_file" >> "$output_log"
    echo "" >> "$output_log"

    rm -f "$temp_file"
}

# ---------------------------
# Запуск curl/wget
# ---------------------------
if [[ "$TARGET" == "curl" || "$TARGET" == "all" ]]; then
    echo "[demo] Running ${#URLS[@]} curl requests..."
    for u in "${URLS[@]}"; do
        # curl выводим -o /dev/null чтобы тело не мешало, но заголовки/статус и [SPY] будут в логе
        run_and_capture "curl -L '$u' -o /dev/null" "CURL $u" "$CUR_LOG"
    done
fi

if [[ "$TARGET" == "wget" || "$TARGET" == "all" ]]; then
    echo "[demo] Running ${#URLS[@]} wget requests..."
    for u in "${URLS[@]}"; do
        run_and_capture "wget '$u' -O /dev/null" "WGET $u" "$WGET_LOG"
    done
fi

# ---------------------------
# Сбор всего вывода и санитизация
# ---------------------------
# теперь FULL_LOG гарантированно содержит весь вывод; читаем его для статистики и очистки
ALL_OUTPUT="$(cat "$FULL_LOG")"

# Санитизация
echo "[demo] Sanitizing logfile -> ${SPY_SAFE:-<no safe path>}"
# Если SPY_SAFE не задан (например, target=curl/wget мы задали его выше), используем default в LOGS_DIR_A
: "${SPY_SAFE:=$LOGS_DIR_A/logs_syscalls_safe.txt}"


sed -E \
  's#/home/[^/]+#~/user#g; s/x-demo-token: .*/x-demo-token: <redacted>/g; s/set-cookie:.*/set-cookie: <redacted>/g' \
  "$FULL_LOG" > "$SPY_SAFE"

echo "[demo] Sanitized logs file: $SPY_SAFE"

# ---------------------------
# Вывод статистики
# ---------------------------
print_statistics "$ALL_OUTPUT" "$TARGET"

echo "[demo] Exit."
