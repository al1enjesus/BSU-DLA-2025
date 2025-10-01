#!/bin/bash
set -e

META_FILE="$HOME/.config/pstat/install_path"

if [ ! -f "$META_FILE" ]; then
    echo "Нет информации о предыдущей установке pstat."
    exit 0
fi

LINK="$(cat "$META_FILE")"

if [ -L "$LINK" ]; then
    echo "[*] Удаляю симлинк $LINK"
    rm -f "$LINK"
else
    echo "[!] Симлинк $LINK не найден, возможно, он уже удалён."
fi

echo "[*] Удаляю файл метаданных $META_FILE"
rm -f "$META_FILE"

echo "[+] Среда очищена."
