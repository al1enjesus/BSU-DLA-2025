# Makefile для Lab 5 - Модули ядра Linux
# Автор: Черноокий Д И
# Группа: 1, Подгруппа: 1, Номер: 20 (Вариант 2)
#
# Использование:
#   make           - Собрать все модули
#   make clean     - Удалить собранные модули
#   make help      - Показать справку

# Имя текущей директории (для out-of-tree сборки)
PWD := $(shell pwd)

# Путь к исходникам ядра
KERNEL_DIR := /lib/modules/$(shell uname -r)/build

# Проверка наличия kernel headers
ifeq ($(wildcard $(KERNEL_DIR)),)
$(error Kernel headers not found at $(KERNEL_DIR). Please install linux-headers-$(shell uname -r))
endif

# Список модулей для сборки (Вариант 2)
obj-m += hello_module.o
obj-m += config_module.o
obj-m += stats_module.o

# Флаги компиляции
ccflags-y := -std=gnu99 -Wall -Wno-declaration-after-statement

# Основная цель - сборка всех модулей
all: check
	@echo "Building kernel modules for Variant 2..."
	@echo "Modules: hello_module, config_module, stats_module"
	@echo ""
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules
	@echo ""
	@echo "✓ Modules built successfully!"
	@echo "  Available modules:"
	@echo "    - hello_module.ko (Task A)"
	@echo "    - config_module.ko (Task B)"
	@echo "    - stats_module.ko (Task C)"
	@echo ""
	@echo "  To load: sudo insmod <module>.ko"
	@echo "  To view logs: dmesg | tail"
	@echo ""

# Проверка окружения
check:
	@echo "=== Environment Check ==="
	@echo "Kernel version: $$(uname -r)"
	@if [ -d "$(KERNEL_DIR)" ]; then \
		echo "Kernel headers: ✓ Found at $(KERNEL_DIR)"; \
	else \
		echo "Kernel headers: ✗ NOT FOUND"; \
		exit 1; \
	fi
	@if command -v gcc &> /dev/null; then \
		echo "Build tools: ✓ gcc $$(gcc --version | head -1 | cut -d' ' -f3)"; \
	else \
		echo "Build tools: ✗ gcc NOT FOUND"; \
		exit 1; \
	fi
	@if [ -f "$(KERNEL_DIR)/Makefile" ]; then \
		echo "Module loading: ✓ insmod available"; \
	else \
		echo "Module loading: ✗ insmod NOT AVAILABLE"; \
		exit 1; \
	fi
	@echo "=========================="
	@echo ""

# Очистка
clean:
	@echo "Cleaning up..."
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
	rm -f *.o *.ko *.mod.c *.mod *.symvers *.order .*.cmd
	rm -rf .tmp_versions
	rm -f Module.symvers modules.order
	@echo "✓ Cleanup complete"

# Справка
help:
	@echo "Makefile for Lab 5 - Kernel Modules"
	@echo ""
	@echo "Usage:"
	@echo "  make           - Build all kernel modules"
	@echo "  make clean     - Remove compiled modules and object files"
	@echo "  make check     - Check build environment"
	@echo "  make help      - Show this help message"
	@echo ""
	@echo "Variant 2 Modules:"
	@echo "  Task A: hello_module.ko - Hello World module"
	@echo "  Task B: config_module.ko - /proc file with read/write"
	@echo "  Task C: stats_module.ko - /proc file with system statistics"
	@echo ""
	@echo "Loading modules (after compilation):"
	@echo "  sudo insmod hello_module.ko"
	@echo "  sudo insmod hello_module.ko message=\"Custom message\""
	@echo "  sudo insmod config_module.ko"
	@echo "  sudo insmod stats_module.ko"
	@echo ""
	@echo "Testing modules:"
	@echo "  cat /proc/my_config          # View current config (Task B)"
	@echo "  echo \"new\" > /proc/my_config # Write to config (Task B)"
	@echo "  cat /proc/sys_stats          # View system stats (Task C)"
	@echo "  dmesg | tail -20             # View kernel logs"
	@echo ""
	@echo "Unloading modules:"
	@echo "  sudo rmmod hello_module"
	@echo "  sudo rmmod config_module"
	@echo "  sudo rmmod stats_module"
	@echo ""

.PHONY: all clean check help
