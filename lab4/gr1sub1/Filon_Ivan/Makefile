CXX = g++
CXXFLAGS = -fPIC -O2 -std=c++17 -Wall -Wextra
LDFLAGS = -shared
TARGET = libsyscall_spy.so
SRC = src/syscall_spy.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	mkdir -p build
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS) -ldl

clean:
	rm -f $(TARGET)
	rm -rf build
