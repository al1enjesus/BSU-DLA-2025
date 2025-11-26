MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
DURATION ?= 5

.PHONY: run
run: build
	@$(MAKEFILE_DIR)bin/pstat $(pid)

.PHONY: build
build:
	@mkdir -p $(MAKEFILE_DIR)bin
	@g++ $(MAKEFILE_DIR)src/pstat.cpp -o $(MAKEFILE_DIR)bin/pstat

.PHONY: test
test: build
	@echo "Starting CPU consumer (dd) in background..."; \
	dd if=/dev/zero of=/dev/null & \
	pid=$$!; \
	echo "Started dd with PID=$$pid"; \
	echo "Sleeping for $(DURATION) seconds..."; \
	sleep $(DURATION); \
	echo; \
	echo "==== running $(MAKEFILE_DIR)bin/pstat $$pid ===="; \
	"$(MAKEFILE_DIR)bin/pstat" $$pid || true; \
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
	echo "Done."

.PHONY: clean
clean:
	rm -rf $(MAKEFILE_DIR)bin
