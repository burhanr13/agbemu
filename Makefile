CC := gcc
CFLAGS := -g -Wall
CPPFLAGS := -I/opt/homebrew/include -MP -MMD
LDFLAGS := $(shell sdl2-config --libs) -lm -lz

BUILD_DIR := build
SRC_DIR := src

TARGET_EXEC := agbemu

SRCS := $(basename $(notdir $(wildcard $(SRC_DIR)/*.c)))
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

.PHONY: debug
debug: $(BUILD_DIR)/$(TARGET_EXEC)

.PHONY: release
release:
	$(CC) -o ./$(TARGET_EXEC) -I/opt/homebrew/include -O3 $(SRCS:%=$(SRC_DIR)/%.c) $(LDFLAGS)

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) -o $@ $(CFLAGS) $(CPPFLAGS) $^ $(LDFLAGS)
	cp $(BUILD_DIR)/$(TARGET_EXEC) ./$(TARGET_EXEC)-dbg

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
