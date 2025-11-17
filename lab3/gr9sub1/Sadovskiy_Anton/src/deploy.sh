#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TARGET="$SCRIPT_DIR/pstat"

# список директорий для установки (по приоритету)
CANDIDATES=(
    "$HOME/.local/bin"
    "/usr/local/bin"
    "/usr/bin"
)

# файл с метаданными (будет лежать в ~/.config/pstat/meta)
META_DIR="$HOME/.config/pstat"
META_FILE="$META_DIR/install_path"

if [ ! -f "$TARGET" ]; then
    echo "Ошибка: не найден исполняемый файл $TARGET"
    exit 1
fi

echo "[*] Делаю исполняемым $TARGET"
chmod +x "$TARGET"

INSTALL_DIR=""
for dir in "${CANDIDATES[@]}"; do
    if [[ -d "$dir" && ":$PATH:" == *":$dir:"* ]]; then
        INSTALL_DIR="$dir"
        break
    fi
done

if [ -z "$INSTALL_DIR" ]; then
    echo "Не удалось найти директорию для установки (нет в PATH)."
    echo "Создаю ~/.local/bin и добавлю туда."
    INSTALL_DIR="$HOME/.local/bin"
    mkdir -p "$INSTALL_DIR"
fi

LINK="$INSTALL_DIR/pstat"

echo "[*] Создаю симлинк $LINK → $TARGET"
mkdir -p "$META_DIR"
ln -sf "$TARGET" "$LINK"

echo "$LINK" > "$META_FILE"

echo "[+] Установлено! Теперь можно запускать 'pstat <pid>' из любой директории."
