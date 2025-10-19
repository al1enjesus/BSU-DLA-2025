#!/usr/bin/env bash
set -e

proj="$PWD"
echo "[1/6] Очистка и сборка..."
make

echo "[2/6] Создание папки logs"
mkdir -p logs

echo "[3/6] Создание большой тестовой папки для find"
rm -rf /tmp/spy_find_test
mkdir -p /tmp/spy_find_test
for d in {1..5}; do
    mkdir -p /tmp/spy_find_test/dir_$d
    for f in {1..20}; do
        echo "file $f" > /tmp/spy_find_test/dir_$d/file_$f.txt
    done
done

echo "[4/6] Запуск find"
LD_PRELOAD=$proj/libsyscall_spy.so find /tmp/spy_find_test 2> logs/find_spy.log || true

echo "[5/6] Создание тестовой папки для tar"
rm -rf /tmp/spy_tar_test
mkdir -p /tmp/spy_tar_test

for d in {1..15}; do
    mkdir -p /tmp/spy_tar_test/dir_$d
    for f in {1..20}; do
        echo "data $f" > /tmp/spy_tar_test/dir_$d/file_$f.txt
    done
done

tar -cf /tmp/spy_tar_test.tar -C /tmp/spy_tar_test .


echo "[6/6] Запуск tar"
LD_PRELOAD=$proj/libsyscall_spy.so tar -tf /tmp/spy_tar_test.tar 2> logs/tar_spy.log || true

echo "[7/8] Создание файлов для cp"
rm -f /tmp/spy_src_*.txt
for i in {1..10}; do
    echo "big content $i" > /tmp/spy_src_$i.txt
done

echo "[8/8] Запуск cp"
rm -f /tmp/spy_dst_*.txt
for i in {1..10}; do
    LD_PRELOAD=$proj/libsyscall_spy.so cp /tmp/spy_src_$i.txt /tmp/spy_dst_$i.txt 2>> logs/cp_spy.log || true
done

echo
echo "ГОТОВО! Логи в папке logs/:"
ls -l logs
