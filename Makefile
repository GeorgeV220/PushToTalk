# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra
INCLUDES := $(shell pkg-config --cflags gtk+-3.0) -I/usr/include/openal -I/usr/include/mpg123
LDFLAGS := $(shell pkg-config --libs gtk+-3.0) -lopenal -lmpg123

# Source files
SRC_DIR := src
SRCS := $(wildcard $(SRC_DIR)/*.cpp) $(wildcard $(SRC_DIR)/gui/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,%.o,$(SRCS))

# Targets
TARGET := pttcpp

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Pattern rule for .cpp -> .o
%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# GUI files need special handling
gui/%.o: $(SRC_DIR)/gui/%.cpp
	@mkdir -p gui
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) gui/*.o

.PHONY: all clean