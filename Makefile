TARGET = mu
LIBTARGET = libmu.a

CC = gcc
AR = ar
SIZE = size

SRC += main.c
SRC += mu.c
SRC += mem.c
SRC += num.c str.c buf.c tbl.c fn.c
SRC += vm.c parse.c
OBJ := $(SRC:.c=.o)
DEP := $(SRC:.c=.d)
ASM := $(SRC:.c=.s)

ifdef DEBUG
CFLAGS += -O0 -g3 -DMU_DEBUG
CFLAGS += -fkeep-inline-functions
else
CFLAGS += -O2
endif
ifdef WORD
CFLAGS += -m$(WORD)
endif
CFLAGS += -std=c99
CFLAGS += -include stdio.h
CFLAGS += -Wall -Winline

LFLAGS += -lm


all: $(TARGET)

lib: $(LIBTARGET)

asm: $(ASM)

size: $(OBJ) $(TARGET)
	$(SIZE) $^

-include $(DEP)


$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $^ $(LFLAGS) -o $@

$(LIBTARGET): $(OBJ)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) -c -MMD $(CFLAGS) $< -o $@

%.s: %.c
	$(CC) -S $(CFLAGS) $< -o $@


clean:
	rm -f $(TARGET) $(LIBTARGET)
	rm -f $(OBJ)
	rm -f $(DEP)
	rm -f $(ASM)
