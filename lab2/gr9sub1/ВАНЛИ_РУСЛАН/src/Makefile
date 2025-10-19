CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -pthread

# Исполняемые файлы
TARGETS := supervisor worker

all: $(TARGETS)

supervisor: supervisor.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

worker: worker.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f $(TARGETS) *.o

.PHONY: all clean

