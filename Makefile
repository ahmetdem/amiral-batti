# Build configuration for the raylove project using local Raylib and ENet

PROJECT_NAME := amiral 

SRC_DIR := src
BIN_DIR := bin
OBJ_DIR := obj
LIB_DIR := lib

CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic
DEBUG_FLAGS := -std=c++17 -Wall -Wextra -Wpedantic -g -DDEBUG
RELEASE_FLAGS := -O3 -DNDEBUG
INCLUDE_DIRS := \
	-I$(SRC_DIR) \
	-I$(LIB_DIR)/raylib/src \
	-I$(LIB_DIR)/raylib/src/external \
	-I$(LIB_DIR)/enet/include

LIBRARIES := \
	$(LIB_DIR)/raylib/src/libraylib.a \
	$(LIB_DIR)/enet/build/libenet.a

PLATFORM_LIBS := -lm -lpthread -ldl
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
	PLATFORM_LIBS := -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
else ifeq ($(UNAME_S),Linux)
	PLATFORM_LIBS += -lrt -lX11 -lGL
endif

SOURCES := $(shell find $(SRC_DIR) -type f -name '*.cc')
OBJECTS := $(patsubst $(SRC_DIR)/%.cc,$(OBJ_DIR)/%.o,$(SOURCES))
DEPS := $(OBJECTS:.o=.d)
TARGET := $(BIN_DIR)/$(PROJECT_NAME)

all: $(TARGET)

$(TARGET): $(OBJECTS) | $(BIN_DIR)
	@echo "Linking $@"
	@$(CXX) $(OBJECTS) -o $@ $(LIBRARIES) $(PLATFORM_LIBS)
	@echo "Build complete: $@"

$(BIN_DIR):
	@mkdir -p $@

$(OBJ_DIR):
	@mkdir -p $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cc | $(OBJ_DIR)
	@echo "Compiling $<"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) -MMD -MP -c $< -o $@

.PHONY: debug
debug: CXXFLAGS := $(DEBUG_FLAGS)
debug: all
	@echo "Debug build complete"

.PHONY: release
release: CXXFLAGS += $(RELEASE_FLAGS)
release: all
	@echo "Release build complete"

.PHONY: run
run: $(TARGET)
	@echo "Running $(PROJECT_NAME) server"
	@cd $(BIN_DIR) && ./$(PROJECT_NAME) 

.PHONY: clean
clean:
	@echo "Cleaning build artifacts"
	@rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: clean-all
clean-all:
	@echo "Cleaning build artifacts and binaries"
	@rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: help
help:
	@echo "Available targets:"
	@echo "  all        - Build the project"
	@echo "  debug      - Build with debug flags"
	@echo "  release    - Build optimized release"
	@echo "  run        - Run the binary"
	@echo "  clean      - Remove object files and executable"
	@echo "  clean-all  - Remove objects and binaries"

-include $(DEPS)
