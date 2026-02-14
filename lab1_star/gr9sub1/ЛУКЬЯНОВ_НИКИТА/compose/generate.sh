#!/usr/bin/env bash
set -euo pipefail

SELF_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COMPOSE_FILE="$SELF_DIR/docker-compose.yml"

echo "[+] Starting stack (build and up)..."
docker compose -f "$COMPOSE_FILE" up -d --build | cat

echo "[+] Generating simple traffic for nginx (if curl is available)..."
if command -v curl >/dev/null 2>&1; then
  for i in $(seq 1 30); do
    curl -s -o /dev/null -w "%{http_code}\n" http://localhost:8080/ >/dev/null || true
    curl -s -o /dev/null -w "%{http_code}\n" http://localhost:8080/notfound >/dev/null || true
    sleep 0.2
  done
else
  echo "curl not found, skipping nginx traffic"
fi

echo "[+] Letting app emit logs for ~30s..."
sleep 30

echo "[+] Done. Now run analysis, for example:"
echo "    bash \"$(cd "$SELF_DIR/.." && pwd)/tools/run.sh\" --from-docker"
echo "[i] Stop stack: docker compose -f \"$COMPOSE_FILE\" down"


