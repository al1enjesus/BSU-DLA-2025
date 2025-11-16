CC = gcc
CFLAGS = -Wall -Wextra -std=c99

build:
	@mkdir -p bin
	$(CC) $(CFLAGS) -o bin/pstat src/pstat.c

run: build
	@if [ -z "$(pid)" ]; then \
		echo "Usage: make run pid=<PID>"; \
		exit 1; \
	fi
	./bin/pstat $(pid)

clean:
	rm -rf bin
