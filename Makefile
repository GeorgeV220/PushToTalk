# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -O2

# Get includes and libs from pkg-config
GTK3_CFLAGS := $(shell pkg-config --cflags gtk+-3.0)
PULSE_CFLAGS := $(shell pkg-config --cflags libpulse)
GTK3_LIBS := $(shell pkg-config --libs gtk+-3.0)
PULSE_LIBS := $(shell pkg-config --libs libpulse)

INCLUDES := $(GTK3_CFLAGS) $(PULSE_CFLAGS) -I/usr/include/openal -I/usr/include/mpg123
LIBS_CLIENT := $(GTK3_LIBS) $(PULSE_LIBS) -lopenal -lmpg123
LIBS_SERVER := -lz

# Source structure
SRC_DIR := src
BUILD_DIR := build

COMMON_SRCS := $(wildcard $(SRC_DIR)/common/**/*.cpp) $(SRC_DIR)/common/utilities/Utility.cpp
CLIENT_SRCS := $(COMMON_SRCS) \
               $(SRC_DIR)/client/client_main.cpp \
               $(SRC_DIR)/client/gui/SettingsGUI.cpp \
               $(SRC_DIR)/client/InputClient.cpp \
               $(SRC_DIR)/client/utilities/AudioUtilities.cpp \
               $(SRC_DIR)/client/push_to_talk.cpp

SERVER_SRCS := $(COMMON_SRCS) \
               $(SRC_DIR)/server/server_main.cpp \
               $(SRC_DIR)/server/device/VirtualInputProxy.cpp

CLIENT_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(CLIENT_SRCS))
SERVER_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SERVER_SRCS))

TARGET_CLIENT := ptt-client
TARGET_SERVER := ptt-server

all: $(TARGET_CLIENT) $(TARGET_SERVER)

# Client target
$(TARGET_CLIENT): $(CLIENT_OBJS)
	@echo "Linking $@..."
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS_CLIENT)

# Server target
$(TARGET_SERVER): $(SERVER_OBJS)
	@echo "Linking $@..."
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS_SERVER)

# Compile rules
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET_CLIENT) $(TARGET_SERVER)

.PHONY: all clean
