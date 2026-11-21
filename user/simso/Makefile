CC ?= cc
LD ?= ld
OBJCOPY ?= objcopy

CFLAGS := -O2 -Wall -Wextra -std=gnu11 -ffreestanding -fno-stack-protector \
          -fno-stack-check -fno-PIC -mno-red-zone \
          -m64 -march=x86-64 -mcmodel=small -I ../lib -I ../../src -I ../../src/sched/contract

.PHONY: all clean

all: SIMSO.BIN

start.o: start.S
	$(CC) $(CFLAGS) -c $< -o $@

simso_render.o: simso_render.c ../../src/sched/contract/sim.h
	$(CC) $(CFLAGS) -c $< -o $@

simso.o: simso.c ../lib/syscall.h ../../src/sched/contract/sim.h
	$(CC) $(CFLAGS) -c $< -o $@

simso.elf: start.o simso_render.o simso.o link.ld
	$(LD) -nostdlib -static -T link.ld start.o simso_render.o simso.o -o $@

SIMSO.BIN: simso.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f start.o simso_render.o simso.o simso.elf SIMSO.BIN
