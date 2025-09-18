#!/usr/bin/env bash
set -euo pipefail

SRC_FILE=""
OUT_FILE=""
WINDOW_MINUTES=30
BURST_MULTIPLIER=3.0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --src)
      SRC_FILE="$2"; shift 2;;
    --out)
      OUT_FILE="$2"; shift 2;;
    --window-minutes)
      WINDOW_MINUTES="$2"; shift 2;;
    --burst-multiplier)
      BURST_MULTIPLIER="$2"; shift 2;;
    -h|--help)
      echo "Usage: $0 --src docker_normalized.csv --out bursts.csv [--window-minutes N] [--burst-multiplier X]"
      exit 0;;
    *)
      echo "Unknown argument: $1" >&2
      exit 1;;
  esac
done

if [[ -z "$SRC_FILE" || -z "$OUT_FILE" ]]; then
  echo "Error: --in and --out are required" >&2
  exit 1
fi

echo "container,minute,count,baseline_avg,multiplier" > "$OUT_FILE"

awk -F, '
  NR>1 {
    minute = substr($1,1,16)
    key = $2 FS minute
    count[key]++
  }
  END {
    for (k in count) {
      split(k, parts, FS)
      container=parts[1]
      minute=parts[2]
      print container, minute, count[k]
    }
  }
' OFS="," "$SRC_FILE" \
| sort -t, -k1,1 -k2,2 \
> "${OUT_FILE}.tmp"

awk -F, -v W="$WINDOW_MINUTES" -v M="$BURST_MULTIPLIER" '
{
  key=$1
  minute=$2
  cnt=$3
  if (key != prev_key) {
    delete hist
    hist_size=0
    prev_key=key
  }
  # считаем среднее по окну
  baseline=0
  if (hist_size>0) {
    sum=0
    for (i in hist) sum+=hist[i]
    baseline=sum/hist_size
  }
  if (baseline>0 && cnt > baseline*M) {
    print key, minute, cnt, baseline, M
  }
  # обновляем историю
  hist[minute]=cnt
  hist_size++
  if (hist_size>W) {
    # удаляем самую старую минуту
    min_key=""
    for (i in hist) {
      if (min_key=="" || i<min_key) min_key=i
    }
    delete hist[min_key]
    hist_size--
  }
}
' OFS="," "${OUT_FILE}.tmp" >> "$OUT_FILE"

rm "${OUT_FILE}.tmp"
