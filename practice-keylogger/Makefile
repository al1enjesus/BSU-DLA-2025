CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -framework ApplicationServices -framework Carbon -framework Foundation -framework CoreFoundation
TARGET = educational_keylogger
SOURCES = KeyLogger.cpp

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET)

.PHONY: all clean