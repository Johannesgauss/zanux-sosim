CC ?= cc
LD ?= ld
OBJCOPY ?= objcopy

CFLAGS := -O2 -Wall -Wextra -std=gnu11 -ffreestanding -fno-stack-protector \
          -fno-stack-check -fno-PIC -mno-red-zone \
          -m64 -march=x86-64 -mcmodel=small \
          -I ../lib -I ../libc/include -I ../../src

.PHONY: all clean

all: VIM.BIN

../libc/libzanuxc.a:
	$(MAKE) -C ../libc

start.o: start.S
	$(CC) $(CFLAGS) -c $< -o $@

vim.o: vim.c ../lib/syscall.h ../libc/include/zgfx.h
	$(CC) $(CFLAGS) -c $< -o $@

vim.elf: start.o vim.o ../libc/libzanuxc.a link.ld
	$(LD) -nostdlib -static -T link.ld start.o vim.o ../libc/libzanuxc.a -o $@

VIM.BIN: vim.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f start.o vim.o vim.elf VIM.BIN
