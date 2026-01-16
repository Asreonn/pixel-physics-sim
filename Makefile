# Pixel-Cell Physics Simulator Makefile
# Supports modular subfolder structure

CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude $(shell pkg-config --cflags sdl2)
LDFLAGS = $(shell pkg-config --libs sdl2) -lm

# Debug build flags
DEBUG_CFLAGS = -Wall -Wextra -g -O0 -DDEBUG -Iinclude $(shell pkg-config --cflags sdl2)

# Directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build

# Find all source files recursively
SRCS = $(shell find $(SRC_DIR) -name '*.c')

# Create object file paths maintaining directory structure
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# Build directories needed
BUILD_SUBDIRS = $(sort $(dir $(OBJS)))

TARGET = pixelsim

.PHONY: all clean debug run dirs

all: dirs $(TARGET)

debug: CFLAGS = $(DEBUG_CFLAGS)
debug: clean all

# Create all necessary build directories
dirs:
	@mkdir -p $(BUILD_SUBDIRS)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Pattern rule for compiling sources in subdirectories
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

# Print source files (for debugging Makefile)
print-srcs:
	@echo "Sources: $(SRCS)"
	@echo "Objects: $(OBJS)"
	@echo "Build dirs: $(BUILD_SUBDIRS)"
