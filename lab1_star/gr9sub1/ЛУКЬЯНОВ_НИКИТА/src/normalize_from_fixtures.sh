#!/usr/bin/env bash
set -euo pipefail

SRC_DIR=""
OUT_FILE=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --src)
      SRC_DIR="$2"; shift 2;;
    --out)
      OUT_FILE="$2"; shift 2;;
    -h|--help)
      echo "Usage: $0 --src <source_dir> --out <output_csv>"
      exit 0;;
    *)
      echo "Unknown argument: $1" >&2
      exit 1;;
  esac
done

  echo "ts_iso,container,stream,message" > "$OUT_FILE"
  
  for f in "$SRC_DIR"/*.log; do
    CONTAINER=$(basename "$f" | sed 's/-json\.log$//')

    jq -r '. | [.time, .stream, .log] | @csv' "$f" \
      | sed 's/"/ /g' \
      | awk -F, -v cname="$CONTAINER" '
          NF >= 3 && $1 != "" {
            gsub(/\n/, " ", $3);
            gsub(/,/, " ", $3);
            print $1 "," cname "," $2 "," $3
          }
        ' >> "$OUT_FILE"
  done

  { head -n1 "$OUT_FILE"; tail -n +2 "$OUT_FILE" | sort -t, -k1,1; } > "$OUT_FILE.tmp"
mv "$OUT_FILE.tmp" "$OUT_FILE"
