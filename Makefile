RAYLIB_PREFIX = /opt/homebrew

CC = cc
CFLAGS = -std=c99 -Wall -Wextra -I$(RAYLIB_PREFIX)/include -Isrc
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

clean:
	rm -rf $(BUILD)

-include $(DEP)

.PHONY: run clean
