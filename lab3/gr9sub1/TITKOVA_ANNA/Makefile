.PHONY: build build-c build-e clean start-imitation monitor-imitation demo scripts


# Для Task C - оригинальная версия
build-c:
	@mkdir -p bin
	go build -o bin/imitation ./src/cmd/imitation/imitation.go
	go build -o bin/pstat ./src/cmd/pstat/pstat.go

# Для Task E* - активная версия  
build-e:
	@mkdir -p bin
	go build -o bin/imitation-active ./src/cmd/imitation-active/imitation-active.go
	go build -o bin/pstat ./src/cmd/pstat/pstat.go

# Clean all
clean:
	rm -rf bin/
	rm -f pstat_io_test.bin test_file_*.bin imitation.log /tmp/imitation.pid

# For manual testing
start-imitation: build-c
	@echo "Starting imitation process..."
	@./bin/imitation & echo $$! > /tmp/imitation.pid
	@echo "Process started with PID: $$(cat /tmp/imitation.pid)"
	@echo "Now run: make monitor-imitation"

monitor-imitation:
	@if [ -f /tmp/imitation.pid ]; then \
		IMITATION_PID=$$(cat /tmp/imitation.pid); \
		if ps -p $$IMITATION_PID > /dev/null 2>&1; then \
			echo "Monitoring PID: $$IMITATION_PID"; \
			./bin/pstat $$IMITATION_PID; \
		else \
			echo "Process not running"; \
			rm -f /tmp/imitation.pid; \
		fi; \
	else \
		echo "No process found. Run: make start-imitation"; \
	fi

# Make scripts executable
scripts:
	chmod +x *.sh

demo: scripts
	@echo "Run one of:"
	@echo "  ./run-task-c.sh    - Part C only"
	@echo "  ./run-task-e.sh    - Part E* only" 
	@echo "  ./run-full-demo.sh - Complete demo"
