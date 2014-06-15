TARGET = v

CC = gcc

SRC := var.c num.c str.c tbl.c fn.c
SRC += mem.c vparse.c vlex.c vm.c
SRC += main.c
OBJ := $(SRC:.c=.o)
ASM := $(OBJ:.o=.s)

#CFLAGS += -O2
#CFLAGS += -O2 -pg
CFLAGS += -O0 -g3 -gdwarf-2 -ggdb
CFLAGS += -finline -foptimize-sibling-calls -freg-struct-return
CFLAGS += -m32
CFLAGS += -Wall -Winline

LFLAGS += -lm


all: $(TARGET)

asm: $(ASM)

v: $(OBJ)
	$(CC) $(CFLAGS) $^ $(LFLAGS) -o $@

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

%.s: %.c
	$(CC) -S -fverbose-asm $(CFLAGS) -o $@ $<

clean:
	-rm $(TARGET)
	-rm $(ASM)
	-rm $(OBJ)
