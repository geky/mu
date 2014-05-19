TARGET = v

CC = gcc
YY = bison
LL = flex

SRC := var.c num.c str.c main.c vlex.c
#FLX := v.l
BSN := v.y
FSC := $(FLX:.l=.lex.c)
BSC := $(BSN:.y=.tab.c)
OBJ := $(BSC:.c=.o) $(FSC:.c=.o) $(SRC:.c=.o)
ASM := $(OBJ:.o=.s)

#CFLAGS += -O3
CFLAGS += -O0 -g3 -gdwarf-2 -ggdb
CFLAGS += -m32
CFLAGS += -Wall
CFLAGS += -lm


all: $(TARGET)

asm: $(ASM)

v: $(BSC) $(FSC) $(OBJ)
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
	-rm $(BSC) $(FSC)
	-rm $(BSN:.y=.tab.h)
