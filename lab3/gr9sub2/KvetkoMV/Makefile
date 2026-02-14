# Makefile for pstat project

BIN_DIR := bin
SRC_DIR := src
SRC := $(wildcard $(SRC_DIR)/*.c)
CC := gcc
CFLAGS := -std=c11 -O2 -Wall -Wextra
TARGET := $(BIN_DIR)/pstat

DURATION ?= 5

.PHONY: all build run test clean

all: build

build: $(TARGET)

$(TARGET): $(SRC)
	@mkdir -p $(BIN_DIR)
	@$(CC) $(CFLAGS) $(SRC) -o $(TARGET)
	@echo "✅ Built $(TARGET)"

run: build
	@if [ -z "$(pid)" ]; then \
		echo "Usage: make run pid=<PID> [json=1]"; exit 1; \
	fi
	@echo "🚀 Running $(TARGET) on PID=$(pid)"
	@if [ "$(json)" = "1" ]; then \
		$(TARGET) $(pid) --json; \
	else \
		$(TARGET) $(pid); \
	fi

test: build
	@echo "🧪 Starting CPU consumer (dd) in background..."
	@dd if=/dev/zero of=/dev/null & \
	pid=$$!; \
	echo "Started dd with PID=$$pid"; \
	echo "Sleeping for $(DURATION) seconds..."; \
	sleep $(DURATION); \
	echo; \
	echo "==== running $(TARGET) $$pid ===="; \
	"$(TARGET)" $$pid || true; \
	echo; \
	echo "==== ps -o ppid,thcount,s,utime,stime,rss -p $$pid ===="; \
	ps -o ppid,thcount,s,utime,stime,rss -p $$pid || true; \
	echo; \
	echo "==== pidstat -p $$pid 1 1 ===="; \
	pidstat -p $$pid 1 1 || true; \
	echo; \
	echo "==== top -b -n 1 -p $$pid ===="; \
	top -b -n 1 -p $$pid || true; \
	echo; \
	echo "Killing $$pid"; \
	kill $$pid 2>/dev/null || true; \
	echo "✅ Done."

clean:
	@rm -rf $(BIN_DIR)
	@echo "🧹 Cleaned build directory."
