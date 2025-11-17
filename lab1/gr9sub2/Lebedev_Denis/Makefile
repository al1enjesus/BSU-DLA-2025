MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

.PHONY: z1
z1:
	@$(MAKEFILE_DIR)src/z1.sh

.PHONY: z2
z2:
	@$(MAKEFILE_DIR)src/z2.sh

.PHONY: z3
z3:
	@$(MAKEFILE_DIR)src/z3.sh

.PHONY: generate_auth
generate_auth:
	@cd $(MAKEFILE_DIR)logs/generator && python3 auth_generator.py