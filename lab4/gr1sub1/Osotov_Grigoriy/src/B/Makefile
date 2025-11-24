CC = g++
CFLAGS = -O2 -Wall
TARGET = benchmark

all: $(TARGET)

$(TARGET): benchmark.cpp
	$(CC) $(CFLAGS) -o $(TARGET) benchmark.cpp

clean:
	rm -f $(TARGET)
