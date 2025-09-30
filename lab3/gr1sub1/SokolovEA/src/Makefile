CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
TARGET = pstat
TARGET_ADV = pstat_advanced
SOURCE = pstat.c
SOURCE_ADV = pstat_advanced.c
OBJDIR = ../obj
BINDIR = ../bin

# Создаем папки если их нет
$(shell mkdir -p $(OBJDIR) $(BINDIR))

.PHONY: all clean install test advanced

all: $(BINDIR)/$(TARGET)

advanced: $(BINDIR)/$(TARGET_ADV)

$(BINDIR)/$(TARGET): $(OBJDIR)/$(SOURCE:.c=.o)
	$(CC) $(CFLAGS) -o $@ $^

$(BINDIR)/$(TARGET_ADV): $(OBJDIR)/$(SOURCE_ADV:.c=.o)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(OBJDIR) $(BINDIR)

install: $(BINDIR)/$(TARGET)
	@echo "Built $(TARGET) successfully!"
	@echo "Run with: $(BINDIR)/$(TARGET) <pid>"

test: $(BINDIR)/$(TARGET)
	@echo "Testing pstat with current shell process (PID: $$$$):"
	$(BINDIR)/$(TARGET) $$$$

help:
	@echo "Available targets:"
	@echo "  all      - Build the basic pstat utility"
	@echo "  advanced - Build the advanced pstat utility with more features"
	@echo "  clean    - Remove build artifacts"
	@echo "  install  - Build and show usage"
	@echo "  test     - Build and test with current shell PID"
	@echo "  help     - Show this help message"