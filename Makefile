TARGET = mu
LIBTARGET = libmu.a

CC = gcc
AR = ar

SRC += mu.c types.c frame.c
SRC += num.c str.c tbl.c fn.c
SRC += mem.c err.c
SRC += lex.c vm.c parse.c
OBJ := $(SRC:.c=.o)
DEP := $(SRC:.c=.d)
ASM := $(SRC:.c=.s)

ifdef DEBUG
CFLAGS += -O0 -g3 -gdwarf-2 -ggdb -DMU_DEBUG
CFLAGS += -fkeep-inline-functions
else
CFLAGS += -O2
endif
CFLAGS += -std=c99
CFLAGS += -include stdio.h
CFLAGS += -foptimize-sibling-calls -freg-struct-return
CFLAGS += -m32
CFLAGS += -Wall -Winline

LFLAGS += -lm


all: $(TARGET)

lib: $(LIBTARGET)

asm: $(ASM)

-include $(DEP)


$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $^ $(LFLAGS) -o $@

$(LIBTARGET): $(OBJ)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) -c -MMD $(CFLAGS) $< -o $@

%.s: %.c
	$(CC) -S -fverbose-asm $(CFLAGS) $< -o $@


clean:
	rm -f $(TARGET) $(LIBTARGET)
	rm -f $(OBJ)
	rm -f $(DEP)
	rm -f $(ASM)
