CC = gcc
CFLAGS = -Wall -O2
SRC = benchmark.c
OUT = benchmark

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $(OUT) $(SRC)

run: $(OUT)
	./$(OUT)

clean:
	rm -f $(OUT)
