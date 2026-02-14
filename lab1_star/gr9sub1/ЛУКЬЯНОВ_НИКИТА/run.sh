#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   bash tools/run.sh --fixtures fixtures
#   bash tools/run.sh --from-docker --since "2025-09-12 00:00" --until "2025-09-12 01:00" --containers "nginx,app"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
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
    bash "$ROOT_DIR/src/normalize_from_fixtures.sh" --src "$ROOT_DIR/$FIXTURES_DIR" --out "$normalized"
  elif [[ $FROM_DOCKER -eq 1 ]]; then
    if ! command -v docker >/dev/null 2>&1; then
      echo "docker is not installed. Use --fixtures instead." >&2; exit 1
    fi
    bash "$ROOT_DIR/src/normalize_from_docker.sh" --out "$normalized" --since "$SINCE" --until "$UNTIL" --containers "$CONTAINERS"
  else
    echo "Specify --fixtures DIR or --from-docker" >&2; exit 1
  fi

  bash "$ROOT_DIR/src/compute_top_ten_errors.sh" --src "$normalized" --out "$OUT_DIR/top_errors.csv"
  bash "$ROOT_DIR/src/compute_bursts.sh" --src "$normalized" --out "$OUT_DIR/bursts.csv" --window-minutes "$WINDOW_MINUTES" --burst-multiplier "$BURST_MULTIPLIER"
  echo "Done. See out/ folder."
}

main "$@"


