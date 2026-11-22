CC ?= cc
LD ?= ld
OBJCOPY ?= objcopy

CFLAGS := -O2 -Wall -Wextra -std=gnu11 -ffreestanding -fno-stack-protector \
          -fno-stack-check -fno-PIC -mno-red-zone \
          -m64 -march=x86-64 -mcmodel=small -I ../lib

.PHONY: all clean

all: SORT.BIN LIFE.BIN PLASMA.BIN

start.o: start.S
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c ../lib/syscall.h
	$(CC) $(CFLAGS) -c $< -o $@

SORT.BIN: start.o sort.o link.ld
	$(LD) -nostdlib -static -T link.ld start.o sort.o -o sort.elf
	$(OBJCOPY) -O binary sort.elf $@

LIFE.BIN: start.o life.o link.ld
	$(LD) -nostdlib -static -T link.ld start.o life.o -o life.elf
	$(OBJCOPY) -O binary life.elf $@

PLASMA.BIN: start.o plasma.o link.ld
	$(LD) -nostdlib -static -T link.ld start.o plasma.o -o plasma.elf
	$(OBJCOPY) -O binary plasma.elf $@

clean:
	rm -f *.o *.elf SORT.BIN LIFE.BIN PLASMA.BIN
