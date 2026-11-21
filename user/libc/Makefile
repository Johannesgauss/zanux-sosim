CC ?= cc
AR ?= ar

CFLAGS := -O2 -Wall -Wextra -std=gnu11 -ffreestanding -fno-stack-protector \
          -fno-stack-check -fno-PIC -mno-red-zone \
          -m64 -march=x86-64 -mcmodel=small -I include -I ../lib -I ../../src

OBJS := string.o stdlib.o stdio.o zgfx.o

.PHONY: all clean

all: libzanuxc.a

%.o: %.c include/string.h include/stdlib.h include/stdio.h include/zgfx.h ../lib/syscall.h
	$(CC) $(CFLAGS) -c $< -o $@

libzanuxc.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

clean:
	rm -f $(OBJS) libzanuxc.a
