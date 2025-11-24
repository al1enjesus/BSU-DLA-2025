#!/bin/bash

# build.sh - Простой скрипт сборки без Makefile
echo "🔧 Простая сборка модулей ядра..."

# Переходим в домашний каталог для работы
WORK_DIR=~/lab5_temp
mkdir -p $WORK_DIR
cd $WORK_DIR

echo "📁 Копируем исходники..."
cp /media/sf_gr1sub1/SokolovEA/src/*.c .
cp /media/sf_gr1sub1/SokolovEA/src/*.h . 2>/dev/null || true

echo "📝 Создаем простой Makefile..."
cat > Makefile << 'EOF'
obj-m += hello_module.o
obj-m += proc_config_module.o  
obj-m += sys_stats_module.o

KDIR = /lib/modules/$(shell uname -r)/build
PWD = $(shell pwd)

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean

install: all
	sudo insmod hello_module.ko
	sudo insmod proc_config_module.ko
	sudo insmod sys_stats_module.ko

remove:
	sudo rmmod hello_module 2>/dev/null || true
	sudo rmmod proc_config_module 2>/dev/null || true
	sudo rmmod sys_stats_module 2>/dev/null || true

test: install
	@echo "=== ТЕСТИРОВАНИЕ ==="
	lsmod | grep -E "(hello_module|proc_config|sys_stats)"
	@echo "--- /proc/my_config ---"
	cat /proc/my_config 2>/dev/null || echo "Файл не найден"
	@echo "test_from_build" | sudo tee /proc/my_config >/dev/null 2>&1
	cat /proc/my_config 2>/dev/null || echo "Файл не найден"
	@echo "--- /proc/sys_stats ---"
	cat /proc/sys_stats 2>/dev/null || echo "Файл не найден"
	@echo "--- dmesg ---"
	dmesg | tail -10

.PHONY: all clean install remove test
EOF

echo "🏗️  Компилируем..."
make clean
make

echo "✅ Результат:"
ls -la *.ko 2>/dev/null || echo "❌ .ko файлы не созданы"

if [ -f "hello_module.ko" ]; then
    echo
    echo "🚀 Запускаем тестирование..."
    make test
    echo
    echo "🧹 Выгружаем модули..."
    make remove
    echo
    echo "✅ Готово! Данные для отчета получены."
else
    echo "❌ Компиляция не удалась."
fi