MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

.PHONY: run
run:
	@mkdir -p $(MAKEFILE_DIR)bin
	@g++ $(MAKEFILE_DIR)src/supervisor.cpp -o $(MAKEFILE_DIR)bin/supervisor -lyaml-cpp
	@g++ $(MAKEFILE_DIR)src/worker.cpp -o $(MAKEFILE_DIR)bin/worker
	@$(MAKEFILE_DIR)bin/supervisor $(MAKEFILE_DIR)src/config.yaml $(MAKEFILE_DIR)bin/worker

.PHONY: term
term:
	@kill -TERM $$(pidof supervisor)

.PHONY: int
int:
	@kill -INT $$(pidof supervisor)

.PHONY: reload
reload:
	@kill -HUP $$(pidof supervisor)

.PHONY: light
light:
	@kill -USR1 $$(pidof supervisor)

.PHONY: heavy
heavy:
	@kill -USR2 $$(pidof supervisor)

.PHONY: clean
clean:
	@rm -rf $(MAKEFILE_DIR)bin