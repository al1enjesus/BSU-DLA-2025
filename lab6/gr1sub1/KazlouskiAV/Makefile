CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=gnu99 `pkg-config --cflags fuse3`
LIBS = `pkg-config --libs fuse3` -lpthread
SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
TARGET = myfuse

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LIBS) -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all clean