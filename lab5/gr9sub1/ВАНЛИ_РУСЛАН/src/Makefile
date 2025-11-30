# [cite_start]Получаем текущую директорию [cite: 1]
PWD := $(shell pwd)

# [cite_start]Путь к заголовкам ядра [cite: 1]
KERNEL_DIR := /lib/modules/$(shell uname -r)/build

# Определяем модули для сборки
obj-m += hello_module.o
obj-m += proc_module.o
obj-m += chardev_module.o

# [cite_start]Флаги компиляции (стандарт C99, подавление предупреждений о смешивании кода и объявлений) [cite: 1]
ccflags-y := -std=gnu99 -Wno-declaration-after-statement

# --- Основные цели ---

all:
	  @echo "Building kernel modules..."
	  $(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules
	  @echo ""
	  @echo "✓ Modules built successfully!"
	  @echo "  To load: sudo insmod <module>.ko"
	  @echo "  To view logs: dmesg | tail"
	  @echo ""

clean:
	  @echo "Cleaning up..."
	  $(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
	  rm -f *.o *.ko *.mod.c *.mod *.symvers *.order .*.cmd
	  rm -rf .tmp_versions
	  @echo "✓ Cleanup complete"

# --- Утилиты загрузки/выгрузки ---

load:
	  @if [ -z "$(MODULE)" ]; then \
	    echo "Error: Please specify MODULE=<module_name>"; \
	    echo "Example: make load MODULE=hello_module"; \
	    exit 1; \
	  fi
	  @if [ ! -f $(MODULE).ko ]; then \
	    echo "Error: $(MODULE).ko not found. Run 'make' first."; \
	    exit 1; \
	  fi
	  @echo "Loading $(MODULE).ko..."
	  sudo insmod $(MODULE).ko
	  @echo "✓ Module loaded. Check 'dmesg | tail' for output"

unload:
	  @if [ -z "$(MODULE)" ]; then \
	    echo "Error: Please specify MODULE=<module_name>"; \
	    echo "Example: make unload MODULE=hello_module"; \
	    exit 1; \
	  fi
	  @echo "Unloading $(MODULE)..."
	  sudo rmmod $(MODULE)
	  @echo "✓ Module unloaded. Check 'dmesg | tail' for output"

info:
	  @echo "=== Loaded Modules ==="
	  @lsmod | head -1
	  @lsmod | grep -E "(hello_module|proc_module|chardev_module)"  echo "No lab modules loaded"
	  @echo ""
	  @echo "=== Built Modules ==="
	  @ls -lh *.ko 2>/dev/null  echo "No modules built yet (run 'make')"

# --- Тесты для каждого модуля ---

test-hello: all
	  @echo "=== Testing hello_module ==="
	  @echo "1. Loading module with default parameter..."
	  -@sudo rmmod hello_module 2>/dev/null  true
	  sudo insmod hello_module.ko
	  @sleep 1
	  @echo "2. Checking dmesg output:"
	  sudo dmesg | tail -5 | grep "custom_hello"
	  @echo "3. Unloading module..."
	  sudo rmmod hello_module
	  @echo "4. Loading module with custom parameter..."
	  sudo insmod hello_module.ko input_name="Tester"
	  @sleep 1
	  @echo "5. Checking dmesg output:"
	  sudo dmesg | tail -5 | grep "custom_hello"
	  sudo rmmod hello_module
	  @echo "✓ Test complete"

test-proc: all
	  @echo "=== Testing proc_module ==="
	  @echo "1. Loading module..."
	  -@sudo rmmod proc_module 2>/dev/null  true
	  sudo insmod proc_module.ko
	  @sleep 1
	  @echo "2. Reading /proc/kernel_task_info:"
	  @if [ -f /proc/kernel_task_info ]; then \
	    cat /proc/kernel_task_info; \
	  else \
	    echo "Error: Proc file not found!"; \
	  fi
	  @echo "3. Unloading module..."
	  sudo rmmod proc_module
	  @echo "4. Checking exit logs:"
	  sudo dmesg | tail -5 | grep "kernel_task"
	  @echo "✓ Test complete"

test-chardev: all
	  @echo "=== Testing chardev_module ==="
	  @echo "1. Loading module..."
	  -@sudo rmmod chardev_module 2>/dev/null  true
	  # Удаляем старую ноду, если она осталась
	  -@sudo rm /dev/simple_char_driver 2>/dev/null  true
	  sudo insmod chardev_module.ko
	  @sleep 1
	  
	  @echo "2. Extracting Major Number..."
	  # Автоматически парсим dmesg, ищем строку "Major number allocated: X"
	  $(eval MAJOR := $(shell sudo dmesg | grep "simple_driver: Major number allocated" | tail -1 | awk -F': ' '{print $$2}'))
	  @if [ -z "$(MAJOR)" ]; then \
	    echo "Error: Could not find major number in dmesg."; \
	    exit 1; \
	  else \
	    echo "Found Major Number: $(MAJOR)"; \
	  fi

	  @echo "3. Creating device node /dev/simple_char_driver..."
	  sudo mknod /dev/simple_char_driver c $(MAJOR) 0
	  sudo chmod 666 /dev/simple_char_driver

	  @echo "4. Writing to device..."
	  echo "Hello Kernel!" > /dev/simple_char_driver
	  
	  @echo "5. Reading from device:"
	  cat /dev/simple_char_driver
	  @echo ""
	  @echo "6. Cleanup..."
	  sudo rm /dev/simple_char_driver
	  sudo rmmod chardev_module
	  @echo "✓ Test complete"

# --- Проверка окружения ---

check:
	  @echo "=== Environment Check ==="
	  @echo -n "Kernel version: "
	  @uname -r
	  @echo -n "Kernel headers: "
	  @if [ -d "$(KERNEL_DIR)" ]; then \
	    echo "✓ Found at $(KERNEL_DIR)"; \
	  else \
	    echo "✗ NOT FOUND"; \
	    echo "  Install with: sudo apt install linux-headers-$$(uname -r)"; \
	  fi
	  @echo -n "Compiler (gcc): "
	  @which gcc > /dev/null && echo "✓ Installed"  echo "✗ Missing"
	  @echo -n "Module utilities: "
	  @which insmod > /dev/null && echo "✓ Installed"  echo "✗ Missing"

help:
	  @echo "Kernel Modules Makefile Helper"
	  @echo ""
	  @echo "Targets:"
	  @echo "  make              - Build all modules"
	  @echo "  make clean        - Remove build artifacts"
	  @echo "  make check        - Verify development environment"
	  @echo ""
	  @echo "Automated Tests:"
	  @echo "  make test-hello   - Build, load, check logs, unload hello_module"
	  @echo "  make test-proc    - Build, load, cat /proc file, unload proc_module"
	  @echo "  make test-chardev - Build, load, mknod, write/read, unload chardev_module"

.PHONY: all clean load unload info test-hello test-proc test-chardev check help
