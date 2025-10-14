CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
TARGET = pstat
SOURCE = pstat.c
OBJDIR = ../obj
BINDIR = ../bin

# Создаем папки если их нет
$(shell mkdir -p $(OBJDIR) $(BINDIR))

.PHONY: all clean install test

all: $(BINDIR)/$(TARGET)

$(BINDIR)/$(TARGET): $(OBJDIR)/$(SOURCE:.c=.o)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(OBJDIR) $(BINDIR)

install: $(BINDIR)/$(TARGET)
	@echo "Built $(TARGET) successfully!"
	@echo "Run with: $(BINDIR)/$(TARGET) <pid>"
	@echo "Available options: -v (verbose), -j (json), -m (monitor), -h (help)"

test: $(BINDIR)/$(TARGET)
	@echo "Testing pstat with current shell process (PID: $$$$):"
	$(BINDIR)/$(TARGET) $$$$
	@echo
	@echo "Testing JSON output:"
	$(BINDIR)/$(TARGET) -j $$$$

help:
	@echo "Available targets:"
	@echo "  all     - Build the pstat utility"
	@echo "  clean   - Remove build artifacts"
	@echo "  install - Build and show usage"
	@echo "  test    - Build and test with current shell PID"
	@echo "  help    - Show this help message"