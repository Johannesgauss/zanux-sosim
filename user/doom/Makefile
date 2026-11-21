CC ?= cc
LD ?= ld
OBJCOPY ?= objcopy

DOOM_DIR ?= $(HOME)/doom_port/src_doom

CFLAGS := -O3 -funroll-loops -std=gnu11 -ffreestanding -fno-stack-protector \
          -fno-stack-check -fno-PIC -mno-red-zone -mno-sse -mno-mmx -mno-80387 \
          -m64 -march=x86-64 -mcmodel=small \
          -I $(DOOM_DIR)/libc -I $(DOOM_DIR) -I ../lib

ALL_DOOM_SRCS := $(wildcard $(DOOM_DIR)/*.c)
DOOM_SRCS := $(filter-out $(DOOM_DIR)/doomgeneric_zanux.c $(DOOM_DIR)/doom_libc.c $(DOOM_DIR)/i_oplmusic.c,$(ALL_DOOM_SRCS))
DOOM_OBJS := $(patsubst $(DOOM_DIR)/%.c,obj/%.o,$(DOOM_SRCS))

LOCAL_OBJS := obj/start.o obj/doom_user_platform.o obj/doom_user_libc.o obj/doom_user_oplmusic.o obj/memory.o

.PHONY: all clean

all: DOOM.BIN

obj:
	mkdir -p obj

obj/start.o: start.S | obj
	$(CC) $(CFLAGS) -c $< -o $@

obj/doom_user_platform.o: doom_user_platform.c ../lib/syscall.h | obj
	$(CC) $(CFLAGS) -c $< -o $@

obj/doom_user_libc.o: doom_user_libc.c ../lib/syscall.h | obj
	$(CC) $(CFLAGS) -c $< -o $@

obj/doom_user_oplmusic.o: doom_user_oplmusic.c ../lib/syscall.h | obj
	$(CC) $(CFLAGS) -c $< -o $@

obj/memory.o: ../lib/memory.c | obj
	$(CC) $(CFLAGS) -c $< -o $@

obj/%.o: $(DOOM_DIR)/%.c | obj
	$(CC) $(CFLAGS) -c $< -o $@

doom.elf: $(LOCAL_OBJS) $(DOOM_OBJS) link.ld
	$(LD) -nostdlib -static -T link.ld $(LOCAL_OBJS) $(DOOM_OBJS) -o $@

DOOM.BIN: doom.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf obj doom.elf DOOM.BIN
