
CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall
TARGET := pstat
SRC := pstat.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	@echo "Usage: ./pstat <pid>"
	@echo "Example: ./pstat $$PPID"

clean:
	rm -f $(TARGET)

