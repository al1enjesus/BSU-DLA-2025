CC = gcc
CFLAGS = -Wall -Wextra -std=c99
TARGET = pstat
SRCDIR = src
SOURCES = $(SRCDIR)/pstat.c

build:
	@mkdir -p bin
	$(CC) $(CFLAGS) -o bin/$(TARGET) $(SOURCES)

run: build
	@if [ -z "$(pid)" ]; then \
		echo "Usage: make run pid=<PID>"; \
		exit 1; \
	fi
	./bin/$(TARGET) $(pid)

clean:
	rm -rf bin
