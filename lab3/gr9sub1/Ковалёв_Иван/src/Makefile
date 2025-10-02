CXX      ?= g++
MODE     ?= release
PREFIX   ?= /usr/local

ifeq ($(MODE),release)
CXXFLAGS   := -std=c++17 -O2 -march=native -pipe -Wall -Wextra -Wno-unused-parameter -DNDEBUG
else ifeq ($(MODE),debug)
CXXFLAGS   := -std=c++17 -g -O0 -pipe -Wall -Wextra -fsanitize=address,undefined -fno-omit-frame-pointer
else
$(error Unsupported MODE '$(MODE)'. Use MODE=release or MODE=debug)
endif

LDFLAGS   ?=
BUILD_DIR := build
BIN_DIR   := bin
TARGET    := $(BIN_DIR)/pstat

SRC_DIR := src
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
ifeq ($(SRCS),)
SRCS := $(wildcard *.cpp)
endif

OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(notdir $(SRCS)))

.PHONY: all clean distclean run install uninstall dirs

all: $(TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	@echo "Linking $@"
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	@echo "Compiling $< -> $@"
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	@echo "Compiling $< -> $@"
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

run: all
ifndef PID
	@echo "Usage: make run PID=<pid>"
	@exit 1
endif
	@echo "Running $(TARGET) $(PID)"
	@$(TARGET) $(PID)

install: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 0755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/pstat
	@echo "Installed to $(DESTDIR)$(PREFIX)/bin/pstat"

uninstall:
	-rm -f $(DESTDIR)$(PREFIX)/bin/pstat
	@echo "Uninstalled from $(DESTDIR)$(PREFIX)/bin/pstat"

clean:
	@rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "Clean done."

distclean: clean

info:
	@echo "CXX      = $(CXX)"
	@echo "MODE     = $(MODE)"
	@echo "CXXFLAGS = $(CXXFLAGS)"
	@echo "SRCS     = $(SRCS)"
	@echo "OBJS     = $(OBJS)"
	@echo "TARGET   = $(TARGET)"

# Простая цель для быстрой проверки (собрать и показать version)
# Предполагается, что pstat при вызове с --version/--help печатает что-то осмысленное.
smoke: all
	@echo "Smoke test (help output):"
	@$(TARGET) --help || true
