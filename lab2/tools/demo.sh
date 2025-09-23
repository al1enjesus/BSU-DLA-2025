#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

# Cross-platform demo (Linux/macOS) to showcase CPU scheduling (nice) and memory behavior.

script_dir="$(cd "$(dirname "$0")" && pwd)"
lab_dir="$(cd "${script_dir}/.." && pwd)"
samples_dir="${lab_dir}/samples"

is_macos() { [[ "$(uname -s)" == "Darwin" ]]; }
is_linux() { [[ "$(uname -s)" == "Linux" ]]; }

say() { echo "[demo] $*"; }

print_usage() {
  cat <<USAGE
Usage: $(basename "$0") [options]

Options:
  --cpu-only                  Run only CPU demo
  --mem-only                  Run only Memory demo
  --cpu-duration SEC          CPU demo duration per process (default: 20)
  --heavy-work-us N           Heavy mode work micros (default: 9000)
  --heavy-sleep-us N          Heavy mode sleep micros (default: 1000)
  --light-work-us N           Light mode work micros (default: 2000)
  --light-sleep-us N          Light mode sleep micros (default: 8000)
  --rss-mb N                  mem_touch target RSS MB (default: 512)
  --step-mb N                 mem_touch step MB (default: 64)
  --sleep-ms N                mem_touch sleep between steps ms (default: 200)
  --pin                       Linux only: pin both cpu_burn to CPU 0 (if taskset present)
  -h, --help                  Show this help
USAGE
}

run_or_warn() {
  local cmd=("$@")
  if ! command -v "${cmd[0]}" >/dev/null 2>&1; then
    say "skip: ${cmd[*]} (not found)"
    return 0
  fi
  "${cmd[@]}"
}

ensure_built() {
  say "Building samples in ${samples_dir} ..."
  make -C "${samples_dir}" | cat
}

cleanup_pids=()
cleanup() {
  set +e
  for p in "${cleanup_pids[@]:-}"; do
    if [[ -n "${p}" ]] && kill -0 "${p}" 2>/dev/null; then
      say "Stopping PID ${p}"
      kill "${p}" 2>/dev/null || true
    fi
  done
}
trap cleanup EXIT INT TERM

# Defaults (can be overridden by CLI)
DO_CPU=true
DO_MEM=true
CPU_DURATION=20
HEAVY_WORK_US=9000
HEAVY_SLEEP_US=1000
LIGHT_WORK_US=2000
LIGHT_SLEEP_US=8000
RSS_MB=512
STEP_MB=64
SLEEP_MS=200
PIN=false

# Parse CLI
while [[ $# -gt 0 ]]; do
  case "$1" in
    --cpu-only) DO_MEM=false ;;
    --mem-only) DO_CPU=false ;;
    --cpu-duration) CPU_DURATION="$2"; shift ;;
    --heavy-work-us|--work-us) HEAVY_WORK_US="$2"; shift ;;
    --heavy-sleep-us|--sleep-us) HEAVY_SLEEP_US="$2"; shift ;;
    --light-work-us) LIGHT_WORK_US="$2"; shift ;;
    --light-sleep-us) LIGHT_SLEEP_US="$2"; shift ;;
    --rss-mb) RSS_MB="$2"; shift ;;
    --step-mb) STEP_MB="$2"; shift ;;
    --sleep-ms) SLEEP_MS="$2"; shift ;;
    --pin) PIN=true ;;
    -h|--help) print_usage; exit 0 ;;
    *) echo "Unknown option: $1"; print_usage; exit 1 ;;
  esac
  shift
done

cpu_demo() {
  say "=== CPU demo: two cpu_burn processes, renice, observe ==="

  local cpu_burn="${samples_dir}/cpu_burn"
  if [[ ! -x "${cpu_burn}" ]]; then
    say "cpu_burn not found/executable: ${cpu_burn}"
    exit 1
  fi

  # Start two workers
  "${cpu_burn}" --duration "${CPU_DURATION}" \
                 --work-us "${HEAVY_WORK_US}" --sleep-us "${HEAVY_SLEEP_US}" >/dev/null &
  local PID1=$!
  cleanup_pids+=("${PID1}")
  "${cpu_burn}" --duration "${CPU_DURATION}" \
                 --light-work-us "${LIGHT_WORK_US}" --light-sleep-us "${LIGHT_SLEEP_US}" >/dev/null &
  local PID2=$!
  cleanup_pids+=("${PID2}")

  say "PIDs: PID1=${PID1}, PID2=${PID2}"
  sleep 1

  if is_linux; then
    # Optional: pin both to CPU 0 if requested and taskset exists (to emphasize competition)
    if ${PIN} && command -v taskset >/dev/null 2>&1; then
      say "Pin both processes to CPU 0 (taskset)"
      taskset -pc 0 "${PID1}" | cat
      taskset -pc 0 "${PID2}" | cat
    fi
  fi

  say "Before renice: per-process snapshot"
  if is_macos; then
    ps -o pid,comm,pcpu,pmem -p "${PID1}" -p "${PID2}"
    run_or_warn top -l 1 -pid "${PID1}" -pid "${PID2}" -stats pid,command,cpu,mem,vsize,compressed | sed -n '1,40p'
  else
    ps -o pid,comm,pcpu,pmem -p "${PID1}" -p "${PID2}"
    if command -v pidstat >/dev/null 2>&1; then
      pidstat -u -p "${PID1},${PID2}" 1 3 | cat
    fi
    top -b -n 1 -p "${PID1}" -p "${PID2}" | sed -n '1,40p'
  fi

  say "Make PID1 more nice (lower CPU share under contention)"
  renice +10 -p "${PID1}" | cat || true
  sleep 2

  say "Toggle PID1 to light, then back to heavy"
  kill -USR1 "${PID1}" 2>/dev/null || true
  sleep 2
  kill -USR2 "${PID1}" 2>/dev/null || true

  say "After renice: per-process snapshot"
  if is_macos; then
    ps -o pid,comm,pcpu,pmem -p "${PID1}" -p "${PID2}"
    run_or_warn top -l 1 -pid "${PID1}" -pid "${PID2}" -stats pid,command,cpu
  else
    ps -o pid,comm,pcpu,pmem -p "${PID1}" -p "${PID2}"
    if command -v pidstat >/dev/null 2>&1; then
      pidstat -u -p "${PID1},${PID2}" 1 3 | cat
    fi
    top -b -n 1 -p "${PID1}" -p "${PID2}" | sed -n '1,40p'
  fi

  say "CPU demo running... waiting a bit"
  sleep 5
}

mem_demo() {
  say "=== Memory demo: mem_touch grows RSS, observe system and per-process ==="
  local mem_touch="${samples_dir}/mem_touch"
  if [[ ! -x "${mem_touch}" ]]; then
    say "mem_touch not found/executable: ${mem_touch}"
    exit 1
  fi

  "${mem_touch}" --rss-mb "${RSS_MB}" --step-mb "${STEP_MB}" --sleep-ms "${SLEEP_MS}" >/dev/null &
  local MPID=$!
  cleanup_pids+=("${MPID}")
  say "MPID=${MPID}"
  sleep 1

  say "Per-process RSS/VSZ"
  ps -o pid,comm,rss,vsz -p "${MPID}"

  say "System view (memory)"
  if is_macos; then
    top -l 1 | grep PhysMem || true
    run_or_warn vm_stat | egrep 'Pages (free|inactive|speculative|occupied by compressor|stored in compressor)' || true
    run_or_warn sysctl vm.swapusage || true
    # Estimate Available ≈ free+inactive+speculative
    local pg
    pg=$(pagesize)
    vm_stat | awk -v pg="${pg}" '/Pages (free|inactive|speculative)/{gsub("\\.","",$3); s+=$3} END{printf "Available ≈ %.2f GB\n", s*pg/1073741824}' || true
  else
    run_or_warn free -h || true
    run_or_warn vmstat 1 3 | cat || true
    head -n 20 /proc/meminfo | cat
  fi

  say "Signal: add one step, then remove one step"
  kill -USR1 "${MPID}" 2>/dev/null || true
  sleep 1
  ps -o pid,comm,rss,vsz -p "${MPID}"
  kill -USR2 "${MPID}" 2>/dev/null || true
  sleep 1
  ps -o pid,comm,rss,vsz -p "${MPID}"

  say "Stopping mem_touch"
  kill "${MPID}" 2>/dev/null || true
  sleep 1
}

main() {
  say "Lab dir: ${lab_dir}"
  ensure_built
  ${DO_CPU} && cpu_demo
  ${DO_MEM} && mem_demo
  say "Done."
}

main "$@"


