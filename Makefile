TARGET = v

CC = gcc
YY = bison

SRC := var.c num.c str.c tbl.c fn.c
SRC += mem.c main.c vlex.c
BSN := vparse.y
FSC := $(FLX:.l=.lex.c)
BSC := $(BSN:.y=.tab.c)
OBJ := $(BSC:.c=.o) $(SRC:.c=.o)
ASM := $(OBJ:.o=.s)

#CFLAGS += -O2
#CFLAGS += -O2 -pg
CFLAGS += -O0 -g3 -gdwarf-2 -ggdb
CFLAGS += -m32
CFLAGS += -Wall -Winline
CFLAGS += -lm


all: $(TARGET)

asm: $(ASM)

v: $(OBJ)
	$(CC) $(OBJ) $(CFLAGS) -o $@

%.output: %.y
	$(YY) -v -t -d $<

%.tab.c: %.y
	$(YY) -d -o $@ $<

%.lex.c: %.l
	$(LL) -o $@ $<

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

%.s: %.c
	$(CC) -S $(CFLAGS) -o $@ $<

clean:
	-rm $(TARGET)
	-rm $(ASM)
	-rm $(OBJ)
	-rm $(BSC)
	-rm $(BSN:.y=.tab.h)
