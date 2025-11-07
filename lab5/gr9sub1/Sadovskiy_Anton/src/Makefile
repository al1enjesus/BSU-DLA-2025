 
# Makefile для сборки всех модулей ядра
# Лабораторная работа №5 - Модули ядра Linux

# Объектные файлы модулей
obj-m += hello_module.o
obj-m += proc_config_module.o
obj-m += proc_stats_module.o

# Директория с исходниками ядра
KDIR := /lib/modules/$(shell uname -r)/build

# Текущая директория
PWD := $(shell pwd)

# Цель по умолчанию - собрать все модули
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# Очистка скомпилированных файлов
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f *.ko *.o *.mod.* *.symvers *.order .*.cmd

# Установка модулей (загрузка в ядро)
install:
	@echo "=== Установка модулей ==="
	@echo "ВНИМАНИЕ: Эта команда загрузит модули в ядро!"
	sudo insmod hello_module.ko
	@echo "hello_module загружен"

# Удаление модулей
uninstall:
	@echo "=== Удаление модулей ==="
	-sudo rmmod hello_module 2>/dev/null || true
	-sudo rmmod proc_config_module 2>/dev/null || true
	-sudo rmmod proc_stats_module 2>/dev/null || true
	@echo "Модули выгружены"

# Проверка загруженных модулей
check:
	@echo "=== Загруженные модули ==="
	lsmod | grep -E "hello_module|proc_config|proc_stats" || echo "Модули не загружены"
	@echo ""
	@echo "=== Последние сообщения ядра ==="
	dmesg | tail -20

# Информация о модулях
info:
	@echo "=== Информация о hello_module.ko ==="
	@modinfo hello_module.ko 2>/dev/null || echo "Модуль не собран"
	@echo ""
	@echo "=== Информация о proc_config_module.ko ==="
	@modinfo proc_config_module.ko 2>/dev/null || echo "Модуль не собран"
	@echo ""
	@echo "=== Информация о proc_stats_module.ko ==="
	@modinfo proc_stats_module.ko 2>/dev/null || echo "Модуль не собран"

# Полная очистка включая логи
distclean: clean uninstall
	@echo "Полная очистка завершена"

.PHONY: all clean install uninstall check info distclean
