#!/usr/bin/env bash
set -euo pipefail

OUT_FILE=""
SINCE=""
UNTIL=""
CONTAINERS=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --out)
      OUT_FILE="$2"; shift 2;;
    --since)
      SINCE="$2"; shift 2;;
    --until)
      UNTIL="$2"; shift 2;;
    --containers)
      CONTAINERS="$2"; shift 2;;
    -h|--help)
      echo "Usage: $0 [--out "file_name"] [--since 'YYYY-MM-DD HH:MM'] [--until 'YYYY-MM-DD HH:MM'] [--containers 'c1,c2']"
      exit 0;;
    *)
      echo "Unknown argument: $1" >&2
      exit 1;;
  esac
done

echo "ts_iso,container,stream,message" > "$OUT_FILE"
  container_list=""
  if [[ -n "$CONTAINERS" ]]; then
    container_list=$(echo "$CONTAINERS" | tr ',' ' ')
  else
    container_list=$(docker ps -a --format '{{.Names}}')
  fi
  for name in $container_list; do
    
    if [[ -n "$SINCE" ]]; then 
      since_arg=(--since "$SINCE"); 
    else 
      since_arg=(); 
    fi
    
    if [[ -n "$UNTIL" ]]; then 
      until_arg=(--until "$UNTIL"); 
    else 
      until_arg=(); 
    fi
      
    docker logs --timestamps "${since_arg[@]}" "${until_arg[@]}" "$name" 2>&1 |
      awk -v container_name="$name" 'BEGIN{OFS=","}
        {
          t=$1; $1=""; sub(/^ +/, "", $0); msg=$0
          stream="stdout"
          if (msg ~ /^(ERROR|Error|error|FATAL|Fatal|fatal|PANIC|panic)/) stream="stderr"
          gsub(/,/, " ", msg)
          print t, container_name, stream, msg
        }
      ' >> "$OUT_FILE"
  done
  { head -n1 "$OUT_FILE"; tail -n +2 "$OUT_FILE" | sort -t, -k1,1; } > "$OUT_FILE.tmp"
mv "$OUT_FILE.tmp" "$OUT_FILE"
