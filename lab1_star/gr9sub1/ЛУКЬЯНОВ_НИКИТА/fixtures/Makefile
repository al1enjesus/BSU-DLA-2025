LINES ?= 1200

.PHONY: all regen check

all: regen check

regen:
	python3 generate_fixtures.py --lines $(LINES)

check:
	@echo "nginx-json.log lines:" $$(wc -l < nginx-json.log)
	@echo "app-json.log lines:  " $$(wc -l < app-json.log)
	@test $$(wc -l < nginx-json.log) -ge $(LINES)
	@test $$(wc -l < app-json.log) -ge $(LINES)
	@echo "OK"


