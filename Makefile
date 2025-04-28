# Compiler settings
CXX := g++
CXXFLAGS := -std=c++11 -Wall -Wextra -O2

# Source files
SRCS := main.cpp simulator.cpp l1cache.cpp bus.cpp
OBJS := $(SRCS:.cpp=.o)
HEADERS := simulator.h l1cache.h bus.h

# Executable name
TARGET := L1simulate

# Default target
all: $(TARGET)

# Link all object files to create the executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compile each source file to object file
%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f $(OBJS) $(TARGET)

# Run the simulation with example parameters
run: $(TARGET)
	./$(TARGET) -t app1 -s 6 -E 2 -b 5 -o results.txt

.PHONY: all clean run
