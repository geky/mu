TARGET = bin/mu
BIN = bin
BUILD = build


CC = gcc
SIZE = size

DIR += . mu linenoise dis
SRC += $(foreach dir,$(DIR),$(wildcard $(dir)/*.c))
OBJ := $(SRC:%.c=$(BUILD)/%.o)
DEP := $(SRC:%.c=$(BUILD)/%.d)
ASM := $(SRC:%.c=$(BUILD)/%.s)

ifdef DEBUG
CFLAGS += -O0 -g3 -DMU_DEBUG
CFLAGS += -fkeep-inline-functions
else ifdef FAST
CFLAGS += -O3 -DMU_FAST
else # default to SMALL
CFLAGS += -Os -DMU_SMALL
endif
CFLAGS += -I.
CFLAGS += -std=c99 -pedantic
CFLAGS += -Wall -Winline

LFLAGS += -lm


all: $(TARGET)

asm: $(ASM)

size: $(OBJ)
	$(SIZE) -t $^

-include $(DEP)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $^ $(LFLAGS) -o $@

$(BUILD):
	mkdir -p $(BIN) $(addprefix $(BUILD)/,$(DIR))

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) -c -MMD $(CFLAGS) $< -o $@

$(BUILD)/%.s: %.c | $(BUILD)
	$(CC) -S $(CFLAGS) $< -o $@

clean:
	rm -rf $(BIN) $(BUILD)
