TARGET_EXEC := agbemu

CC := gcc

CFLAGS := -Wall -Wimplicit-fallthrough -Wno-format -Werror
CFLAGS_RELEASE := -O3 -flto
CFLAGS_DEBUG := -g

CPPFLAGS := -MP -MMD

LDFLAGS := -lm -lSDL2

ifeq ($(shell uname),Darwin)
	CPPFLAGS += -I$(shell brew --prefix)/include
	LDFLAGS := -L$(shell brew --prefix)/lib $(LDFLAGS)
endif

BUILD_DIR := build
SRC_DIR := src

DEBUG_DIR := $(BUILD_DIR)/debug
RELEASE_DIR := $(BUILD_DIR)/release

SRCS := $(shell find $(SRC_DIR) -name '*.c')
SRCS := $(SRCS:$(SRC_DIR)/%=%)

OBJS_DEBUG := $(SRCS:%.c=$(DEBUG_DIR)/%.o)
DEPS_DEBUG := $(OBJS_DEBUG:.o=.d)

OBJS_RELEASE := $(SRCS:%.c=$(RELEASE_DIR)/%.o)
DEPS_RELEASE := $(OBJS_RELEASE:.o=.d)

.PHONY: release, debug, clean

release: CFLAGS += $(CFLAGS_RELEASE)
release: $(RELEASE_DIR)/$(TARGET_EXEC)

debug: CFLAGS += $(CFLAGS_DEBUG)
debug: $(DEBUG_DIR)/$(TARGET_EXEC)

$(RELEASE_DIR)/$(TARGET_EXEC): $(OBJS_RELEASE)
	$(CC) -o $@ $(CFLAGS) $(CPPFLAGS) $^ $(LDFLAGS)
	cp $@ $(TARGET_EXEC)

$(RELEASE_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(DEBUG_DIR)/$(TARGET_EXEC): $(OBJS_DEBUG)
	$(CC) -o $@ $(CFLAGS) $(CPPFLAGS) $^ $(LDFLAGS)
	cp $@ $(TARGET_EXEC)

$(DEBUG_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET_EXEC)

-include $(DEPS_DEBUG)
-include $(DEPS_RELEASE)
