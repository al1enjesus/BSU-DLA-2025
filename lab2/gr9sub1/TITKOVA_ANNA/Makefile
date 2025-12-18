.PHONY: all build build-a build-b clean run run-a run-b demo demo-a demo-b test

# Пути
SRC_DIR = src
BIN_DIR = bin
CONFIG_DIR = $(SRC_DIR)/config
SCRIPTS_DIR = scripts
LOG_DIR = logs

# Цели
all: build

build-mem-touch:
	@echo "Building mem_touch..."
	gcc -O2 cfiles/mem_touch.c -o $(BIN_DIR)/mem_touch
	@echo "mem_touch built!"

build-cpu-burn:
	@echo "Building cpu_burn..."
	gcc -O2 cfiles/cpu_burn.c -o $(BIN_DIR)/cpu_burn
	@echo "cpu_burn built!"

# Базовая сборка (без копирования конфига)
build-base: build-cpu-burn build-mem-touch
	@echo "Building supervisor and worker..."
	cd $(SRC_DIR)/supervisor && go build -o ../../$(BIN_DIR)/supervisor .
	cd $(SRC_DIR)/worker && go build -o ../../$(BIN_DIR)/worker .
	@echo "Base build complete! Binaries are in: $(BIN_DIR)/"

# Сборка для задания A
build-a: build-base
	@echo "Copying config_a.json for Task A..."
	mkdir -p $(BIN_DIR)/config
	cp $(CONFIG_DIR)/config_a.json $(BIN_DIR)/config/config.json
	@echo "Task A build complete!"

# Сборка для задания B с правильным копированием конфига
build-b: build-base
	@echo "Copying config_b.json for Task B..."
	mkdir -p $(BIN_DIR)/config
	cp $(CONFIG_DIR)/config_b.json $(BIN_DIR)/config/config.json
	@echo "Task B build complete!"


# Сборка по умолчанию (задание A)
build: build-a

clean:
	@echo "Cleaning..."
	rm -rf $(BIN_DIR) $(LOG_DIR) pids
	@echo "Clean complete!"

