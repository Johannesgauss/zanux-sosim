CC ?= cc
LD ?= ld
OBJCOPY ?= objcopy

CFLAGS := -O2 -Wall -Wextra -std=gnu11 -ffreestanding -fno-stack-protector \
          -fno-stack-check -fno-PIC -mno-red-zone \
          -m64 -march=x86-64 -mcmodel=small \
          -I ../lib -I ../libc/include -I ../../src -I ../../src/sched/contract

.PHONY: all clean

all: SIMSO.BIN

../libc/libzanuxc.a:
	$(MAKE) -C ../libc

start.o: start.S
	$(CC) $(CFLAGS) -c $< -o $@

simso_render.o: simso_render.c ../../src/sched/contract/sim.h
	$(CC) $(CFLAGS) -c $< -o $@

simso_json.o: simso_json.c simso_json.h ../../src/sched/contract/sim.h
	$(CC) $(CFLAGS) -c $< -o $@

simso.o: simso.c simso_json.h ../lib/syscall.h ../../src/sched/contract/sim.h
	$(CC) $(CFLAGS) -c $< -o $@

simso.elf: start.o simso_render.o simso_json.o simso.o ../libc/libzanuxc.a link.ld
	$(LD) -nostdlib -static -T link.ld start.o simso_render.o simso_json.o simso.o ../libc/libzanuxc.a -o $@

SIMSO.BIN: simso.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f start.o simso_render.o simso_json.o simso.o simso.elf SIMSO.BIN
