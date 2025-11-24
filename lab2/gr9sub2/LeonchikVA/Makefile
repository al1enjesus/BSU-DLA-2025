CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
# Флаг для sched_getcpu() в worker.c
LDFLAGS = -lrt 

TARGETS = supervisor worker
SRC_DIR = src
BIN_DIR = .

all: $(TARGETS)

supervisor: $(SRC_DIR)/supervisor.c $(SRC_DIR)/config.c $(SRC_DIR)/config.h
	$(CC) $(CFLAGS) $< $(SRC_DIR)/config.c -o $(BIN_DIR)/$@ $(LDFLAGS)

worker: $(SRC_DIR)/worker.c $(SRC_DIR)/config.c $(SRC_DIR)/config.h
	$(CC) $(CFLAGS) $< $(SRC_DIR)/config.c -o $(BIN_DIR)/$@ $(LDFLAGS)

clean:
	rm -f $(BIN_DIR)/$(TARGETS)

.PHONY: all clean