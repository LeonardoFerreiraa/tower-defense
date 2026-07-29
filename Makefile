RAYLIB_PREFIX = /opt/homebrew

CC = clang
CFLAGS = -std=gnu23 -Wall -Wextra -Wimplicit-fallthrough -fms-extensions -I$(RAYLIB_PREFIX)/include -Isrc
LDFLAGS = -L$(RAYLIB_PREFIX)/lib -lraylib

BUILD = build
TARGET = $(BUILD)/game
SRC = $(wildcard src/*.c)
OBJ = $(SRC:src/%.c=$(BUILD)/%.o)
DEP = $(OBJ:.o=.d)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

$(BUILD)/%.o: src/%.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

run: $(TARGET)
	./$(TARGET)

# build com logs de debug e seed fixa (#ifdef DEBUG_ENABLED)
debug: CFLAGS += -DDEBUG_ENABLED -g -O0
debug: clean $(TARGET)

clean:
	rm -rf $(BUILD)

-include $(DEP)

.PHONY: run debug clean
