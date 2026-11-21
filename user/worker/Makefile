CC ?= cc
LD ?= ld
OBJCOPY ?= objcopy

CFLAGS := -O2 -m64 -march=x86-64 -ffreestanding -fno-PIC

.PHONY: all clean

all: WORKER.BIN

worker.o: worker.S
	$(CC) $(CFLAGS) -c $< -o $@

worker.elf: worker.o link.ld
	$(LD) -nostdlib -static -T link.ld worker.o -o $@

WORKER.BIN: worker.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f worker.o worker.elf WORKER.BIN
