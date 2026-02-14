#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   bash tools/run.sh --fixtures fixtures
#   bash tools/run.sh --from-docker --since "2025-09-12 00:00" --until "2025-09-12 01:00" --containers "nginx,app"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
OUT_DIR="$ROOT_DIR/out"
FIXTURES_DIR=""
FROM_DOCKER=0
SINCE=""
UNTIL=""
CONTAINERS=""
WINDOW_MINUTES=30
BURST_MULTIPLIER=3.0

mkdir -p "$OUT_DIR"

print_help() {
  cat <<EOF
Usage: $0 [--fixtures DIR | --from-docker] [--since "YYYY-MM-DD HH:MM"] [--until "YYYY-MM-DD HH:MM"] [--containers "c1,c2"] [--window-minutes N] [--burst-multiplier X]

Outputs:
  out/docker_normalized.csv   ts_iso,container,stream,message
  out/top_errors.csv          container,error_count
  out/bursts.csv              container,minute,count,baseline_avg,multiplier
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --fixtures)
      FIXTURES_DIR="$2"; shift 2;;
    --from-docker)
      FROM_DOCKER=1; shift;;
    --since)
      SINCE="$2"; shift 2;;
    --until)
      UNTIL="$2"; shift 2;;
    --containers)
      CONTAINERS="$2"; shift 2;;
    --window-minutes)
      WINDOW_MINUTES="$2"; shift 2;;
    --burst-multiplier)
      BURST_MULTIPLIER="$2"; shift 2;;
    -h|--help)
      print_help; exit 0;;
    *)
      echo "Unknown arg: $1" >&2; print_help; exit 1;;
  esac
done

normalize_from_fixtures() {
  local src_dir="$1"
  local out_csv="$2"
  # Expect docker json-file like lines: {"log":"...","stream":"stdout","time":"2025-09-12T00:00:01.234567890Z"}
  # Fallback parser without jq: extract keys conservatively.
  awk '
    function json_unquote(s){
      gsub(/^"|"$/, "", s); gsub(/\\n/, " ", s); gsub(/,/, " ", s); return s
    }
    BEGIN { OFS=","; print "ts_iso","container","stream","message" }
    {
      line=$0
      # naive field extraction
      lo=""; stream=""; time=""; container=FILENAME
      if (match(line, /"time"\s*:\s*"[^"]+"/)) { t=substr(line, RSTART, RLENGTH); split(t, a, ":"); time=a[2]; sub(/^\s*"/, "", time); sub(/"\s*$/, "", time) }
      if (match(line, /"stream"\s*:\s*"[^"]+"/)) { s=substr(line, RSTART, RLENGTH); split(s, a, ":"); stream=a[2]; sub(/^\s*"/, "", stream); sub(/"\s*$/, "", stream) }
      if (match(line, /"log"\s*:\s*"[^"]*"/)) { l=substr(line, RSTART, RLENGTH); split(l, a, ":"); lo=substr(l, index(l, ":")+1); sub(/^\s*"/, "", lo); sub(/"\s*$/, "", lo) }
	      gsub(/\\t/, " ", lo); gsub(/\\n/, " ", lo)
      gsub(/,/, " ", lo)
      # derive container name from filename (strip dir and extension)
      n=split(container, parts, "/"); base=parts[n]; sub(/\.[^.]*$/, "", base)
      if (time=="") next
      print time, base, (stream==""?"stdout":stream), lo
    }
  ' "$src_dir"/*.log | sort -t, -k1,1 > "$out_csv"
}

normalize_from_docker() {
  local out_csv="$1"
  : > "$out_csv"
  echo "ts_iso,container,stream,message" > "$out_csv"
  local list
  if [[ -n "$CONTAINERS" ]]; then
    list=$(echo "$CONTAINERS" | tr ',' ' ')
  else
    list=$(docker ps -a --format '{{.Names}}')
  fi
  for name in $list; do
    # docker logs timestamps are RFC3339Nano; use since/until if provided
    if [[ -n "$SINCE" ]]; then since_arg=(--since "$SINCE"); else since_arg=(); fi
    if [[ -n "$UNTIL" ]]; then until_arg=(--until "$UNTIL"); else until_arg=(); fi
    # capture both stdout and stderr by running twice (docker logs does not tag stream in plain output)
    docker logs --timestamps "${since_arg[@]}" "${until_arg[@]}" "$name" 2>&1 |
      awk -v cname="$name" 'BEGIN{OFS=","}
        NR==1 && FNR==1 { }
        {
          ts=$1; $1=""; sub(/^ +/, "", $0); msg=$0
          stream="stdout"
          if (msg ~ /^(ERROR|Error|error|FATAL|Fatal|fatal|PANIC|panic)/) stream="stderr"
          gsub(/,/, " ", msg)
          print ts, cname, stream, msg
        }
      ' >> "$out_csv.tmp"
  done
  sort -t, -k1,1 "$out_csv.tmp" >> "$out_csv"
  rm -f "$out_csv.tmp"
}

compute_top_errors() {
  local in_csv="$1"; local out_csv="$2"
  awk -F, 'BEGIN{OFS=","; print "container","error_count"}
    NR==1{next}
    {
      msg=$4; low=tolower(msg)
      if (low ~ /(error|fail|fatal|panic)/) cnt[$2]++
    }
    END{
      for (c in cnt) print c, cnt[c]
    }
  ' "$in_csv" | sort -t, -k2,2nr > "$out_csv"
}

compute_bursts() {
  local in_csv="$1"; local out_csv="$2"; local window="$3"; local mult="$4"
  awk -F, -v W="$window" -v M="$mult" 'BEGIN{OFS=","; print "container","minute","count","baseline_avg","multiplier"}
    NR==1{next}
    {
      # minute key: YYYY-MM-DDTHH:MM
      minute=substr($1,1,16)
      key=$2"\t"minute
      counts[key]++
    }
    END{
      # collect per container minutes, compute rolling baseline over previous W minutes (approximate: treat as sorted string order)
      # build lists
      for (k in counts) {
        split(k, a, "\t"); c=a[1]; m=a[2]
        idx[c] = idx[c] ":" m
      }
      for (c in idx) {
        split(substr(idx[c],2), arr, ":")
        n=asort(arr)
        for (i=1;i<=n;i++) {
          m=arr[i]; key=c"\t"m; cur=counts[key]
          # baseline: average of last W minutes in array index space
          start=i-W; if (start<1) start=1
          sum=0; cntw=0
          for (j=start;j<i;j++) { mm=arr[j]; kk=c"\t"mm; sum+=counts[kk]; cntw++ }
          base=(cntw>0?sum/cntw:0)
          if (cntw>0 && base>0 && cur >= base*M) {
            print c, m, cur, base, sprintf("%.2f", cur/(base>0?base:1))
          }
        }
      }
    }
  ' "$in_csv" > "$out_csv"
}

main() {
  local normalized="$OUT_DIR/docker_normalized.csv"
  if [[ -n "$FIXTURES_DIR" ]]; then
    normalize_from_fixtures "$ROOT_DIR/$FIXTURES_DIR" "$normalized"
  elif [[ $FROM_DOCKER -eq 1 ]]; then
    if ! command -v docker >/dev/null 2>&1; then
      echo "docker is not installed. Use --fixtures instead." >&2; exit 1
    fi
    normalize_from_docker "$normalized"
  else
    echo "Specify --fixtures DIR or --from-docker" >&2; exit 1
  fi

  compute_top_errors "$normalized" "$OUT_DIR/top_errors.csv"
  compute_bursts "$normalized" "$OUT_DIR/bursts.csv" "$WINDOW_MINUTES" "$BURST_MULTIPLIER"
  echo "Done. See out/ folder."
}

main "$@"


