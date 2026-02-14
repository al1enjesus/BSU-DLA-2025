#!/usr/bin/env bash
set -euo pipefail

SRC_FILE=""
OUT_FILE=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --src)
      SRC_FILE="$2"; shift 2;;
    --out)
      OUT_FILE="$2"; shift 2;;
    -h|--help)
      echo "Usage: $0 --src <source_csv> --out <output_csv>"
      exit 0;;
    *)
      echo "Unknown argument: $1" >&2
      exit 1;;
  esac
done

echo "container,error_count" > "$OUT_FILE"

awk -F, 'BEGIN{IGNORECASE=1}
  NR>1 && $4 ~ /error|fail|fatal|panic/ { cnt[$2]++ }
  END {
    for (c in cnt) print c, cnt[c]
  }
' "$SRC_FILE" \
  | sort -k2 -nr \
  | head -n 10 \
  | awk '{print $1 "," $2}' >> "$OUT_FILE"
