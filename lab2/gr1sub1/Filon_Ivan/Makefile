MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
CXX = g++
CXXFLAGS = -O2 -Wall -std=c++17

.PHONY: run
run:
	@mkdir -p $(MAKEFILE_DIR)bin
	@$(CXX) $(CXXFLAGS) $(MAKEFILE_DIR)src/mini-supervisor.cpp -o $(MAKEFILE_DIR)bin/supervisor -lyaml-cpp
	@$(CXX) $(CXXFLAGS) $(MAKEFILE_DIR)src/worker.cpp -o $(MAKEFILE_DIR)bin/worker
	@$(MAKEFILE_DIR)bin/supervisor $(MAKEFILE_DIR)src/config.yml $(MAKEFILE_DIR)bin/worker

.PHONY: term
term:
	@kill -TERM $$(pidof supervisor)

.PHONY: int
int:
	@kill -INT $$(pidof supervisor)

.PHONY: reload
reload:
	@kill -HUP $$(pidof supervisor)

.PHONY: light-mode
light-mode:
	@kill -USR1 $$(pidof supervisor)

.PHONY: heavy-mode
heavy-mode:
	@kill -USR2 $$(pidof supervisor)

.PHONY: clean
clean:
	@rm -rf $(MAKEFILE_DIR)bin
