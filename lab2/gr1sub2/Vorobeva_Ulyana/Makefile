CXX = g++
CXXFLAGS = -std=c++17 -O2
YAML_LIB = -lyaml-cpp
BIN_DIR = $(CURDIR)/bin
SRC_DIR = $(CURDIR)/src

.PHONY: all run clean term int reload light heavy

all: $(BIN_DIR)/supervisor $(BIN_DIR)/worker

$(BIN_DIR)/supervisor: $(SRC_DIR)/supervisor.cpp
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(YAML_LIB)

$(BIN_DIR)/worker: $(SRC_DIR)/worker.cpp
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

run: all
	$(BIN_DIR)/supervisor $(SRC_DIR)/config.yaml $(BIN_DIR)/worker

term:
	-kill -TERM $$(pidof supervisor)

int:
	-kill -INT $$(pidof supervisor)

reload:
	-kill -HUP $$(pidof supervisor)

light:
	-kill -USR1 $$(pidof supervisor)

heavy:
	-kill -USR2 $$(pidof supervisor)

clean:
	rm -rf $(BIN_DIR)
