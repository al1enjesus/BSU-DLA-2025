PWD := $(shell pwd)

# Путь к исходникам ядра
KERNEL_DIR := /lib/modules/$(shell uname -r)/build

# Список модулей для сборки
obj-m += src/hello_module.o
obj-m += src/proc_module.o
obj-m += src/sys_stats_module.o

# Флаги компиляции (опционально)
ccflags-y := -std=gnu99 -Wno-declaration-after-statement

# Основная цель - сборка всех модулей
all:
	@echo "Building kernel modules..."
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules
	@echo ""
	@echo "✓ Modules built successfully!"
	@echo "  To load: sudo insmod src/<module>.ko"
	@echo "  To view logs: dmesg | tail"
	@echo ""

# Очистка
clean:
	@echo "Cleaning up..."
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
	rm -f src/*.o src/*.ko src/*.mod.c src/*.mod src/*.symvers src/*.order src/.*.cmd
	rm -rf src/.tmp_versions
	@echo "✓ Cleanup complete"

# Проверка окружения
check:
	@echo "=== Environment Check ==="
	@echo -n "Kernel version: "
	@uname -r
	@echo -n "Kernel headers: "
	@if [ -d "$(KERNEL_DIR)" ]; then \
		echo "✓ Found at $(KERNEL_DIR)"; \
	else \
		echo "✗ NOT FOUND"; \
	fi
	@echo -n "Build tools: "
	@which gcc > /dev/null && echo "✓ gcc installed" || echo "✗ gcc NOT installed"

# Справка
help:
	@echo "Lab 5 Kernel Modules - Makefile"
	@echo ""
	@echo "Available targets:"
	@echo "  make              - Build all modules"
	@echo "  make clean        - Remove built modules"
	@echo "  make check        - Check environment"
	@echo "  make help         - Show this help"

.PHONY: all clean check help
