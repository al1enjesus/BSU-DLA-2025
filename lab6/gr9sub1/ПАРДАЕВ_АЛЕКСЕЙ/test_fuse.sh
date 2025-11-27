#!/bin/bash
set -e

# ============================
# Переход в каталог скрипта
# ============================
cd "$(dirname "$0")"

# ============================
# Сборка проекта
# ============================
echo "=== Building project (make) ==="
if make; then
    echo -e "\e[1;32m✓ Build OK\e[0m"
else
    echo -e "\e[1;31m✗ Build failed\e[0m"
    exit 1
fi

# ============================
# Пути и бинарник
# ============================
FUSE=./myfuse
MNT=/tmp/fuse_test_mnt
SRC=/tmp/fuse_test_src
ARC=/tmp/test.tar

print() { echo -e "\e[1;36m$1\e[0m"; }
ok()    { echo -e "\e[1;32m✓ $1\e[0m"; }
fail()  { echo -e "\e[1;31m✗ $1\e[0m"; exit 1; }

cleanup_mount() {
    fusermount -u "$MNT" 2>/dev/null || umount "$MNT" 2>/dev/null || true
}

prepare_env() {
    cleanup_mount
    rm -rf "$SRC" "$MNT" "$ARC"
    mkdir -p "$SRC" "$MNT"
}

###############################################################
# TEST A — PASSTHROUGH FS
###############################################################
test_passthrough() {
    print "=== TEST A: Passthrough FS ==="
    prepare_env

    echo "hello" > "$SRC/hello.txt"
    mkdir "$SRC/dir1"
    echo "inside" > "$SRC/dir1/a.txt"

    $FUSE passthrough "$SRC" "$MNT" -f &
    PID=$!
    sleep 1

    [[ -f "$MNT/hello.txt" ]] && ok "hello.txt exists" || fail "hello.txt missing"

    CONTENT=$(cat "$MNT/hello.txt")
    [[ "$CONTENT" = "hello" ]] && ok "read file" || fail "wrong content"

    echo "world" >> "$MNT/hello.txt"
    CONTENT=$(cat "$SRC/hello.txt")
    [[ "$CONTENT" = "hello\nworld" ]] && ok "write works" || fail "write failed"

    ls "$MNT/dir1" | grep "a.txt" >/dev/null && ok "directory listing" || fail "dir listing failed"

    rm "$MNT/hello.txt"
    [[ ! -f "$SRC/hello.txt" ]] && ok "unlink works" || fail "unlink failed"

    cleanup_mount
    kill "$PID" 2>/dev/null || true
}

###############################################################
# TEST B — ARCHIVE FS
###############################################################
prepare_tar() {
    mkdir /tmp/tar_src
    echo "aaa" >/tmp/tar_src/a.txt
    echo "bbb" >/tmp/tar_src/b.txt
    tar -cf "$ARC" -C /tmp/tar_src .
}

test_archive() {
    print "=== TEST B: Archive FS ==="
    prepare_env
    prepare_tar

    $FUSE archive "$ARC" "$MNT" -f &
    PID=$!
    sleep 1

    ls "$MNT" | grep "a.txt" >/dev/null && ok "a.txt listed" || fail "a.txt missing"
    ls "$MNT" | grep "b.txt" >/dev/null && ok "b.txt listed" || fail "b.txt missing"

    ACONT=$(cat "$MNT/a.txt")
    [[ "$ACONT" = "aaa" ]] && ok "content of a.txt correct" || fail "bad a.txt"

    echo "cannot" > "$MNT/a.txt" 2>/dev/null && fail "archive should be read-only"
    ok "archive is read-only"

    cleanup_mount
    kill "$PID" 2>/dev/null || true
}

###############################################################
# TEST C — MONITORING FS
###############################################################
test_monitoring() {
    print "=== TEST C: Monitoring FS ==="
    prepare_env

    echo "abc" > "$SRC/x.txt"

    $FUSE monitor "$SRC" "$MNT" -f &
    PID=$!
    sleep 1

    cat "$MNT/x.txt" >/dev/null
    echo "123" >> "$MNT/x.txt"

    INFO=$(cat "$MNT/.stats")

    echo "$INFO" | grep -q "open:" && ok ".stats has open" || fail "missing open"
    echo "$INFO" | grep -q "read:" && ok ".stats has read" || fail "missing read"
    echo "$INFO" | grep -q "write:" && ok ".stats has write" || fail "missing write"

    cleanup_mount
    kill "$PID" 2>/dev/null || true
}

###############################################################
# MAIN
###############################################################
print "Running full test suite..."

test_passthrough
test_archive
test_monitoring

print "===================================="
print " ALL TESTS PASSED SUCCESSFULLY ✓✓✓ "
print "===================================="
