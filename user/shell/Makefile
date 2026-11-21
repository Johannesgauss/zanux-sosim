CC ?= cc
LD ?= ld
OBJCOPY ?= objcopy

CFLAGS := -O2 -Wall -Wextra -std=gnu11 -ffreestanding -fno-stack-protector \
          -fno-stack-check -fno-PIC -mno-red-zone -mno-sse -mno-mmx -mno-80387 \
          -m64 -march=x86-64 -mcmodel=small -I ../lib

.PHONY: all clean

all: SHELL.BIN

start.o: start.S
	$(CC) $(CFLAGS) -c $< -o $@

shell.o: shell.c ../lib/syscall.h
	$(CC) $(CFLAGS) -c $< -o $@

shell.elf: start.o shell.o link.ld
	$(LD) -nostdlib -static -T link.ld start.o shell.o -o $@

SHELL.BIN: shell.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f start.o shell.o shell.elf SHELL.BIN
