#include "util/alias.h"
#include "arch/sys/syscall.h"
#include "arch/gdt_macros.h"
#include <stddef.h>
#include <stdint.h>

int64_t sys_write(int fd, const char *buf, size_t count);
int64_t sys_read(int fd, char *buf, size_t count);
int64_t sys_exec(const char *name);
int64_t sys_alloc(size_t size);
int64_t sys_read_file(const char *name, void *dst, size_t max_len);
int64_t sys_fb_blit(const uint32_t *buf, uint32_t w, uint32_t h);
int64_t sys_poll_scancode(void);
int64_t sys_ticks_ms(void);
int64_t sys_pcspk(uint16_t divisor);
int64_t sys_opl_io(uint16_t port, uint8_t val, int is_write);
int64_t sys_readdir(size_t index, char name_out[16], uint32_t *size_out);

typedef int64_t (*sys_call_t)(umax, umax, umax, umax, umax, umax);

#define SYSCALL_NUMBER 12
const sys_call_t syscall_table[SYSCALL_NUMBER] = {
	(sys_call_t)sys_exit,
	(sys_call_t)sys_write,
	(sys_call_t)sys_read,
	(sys_call_t)sys_exec,
	(sys_call_t)sys_alloc,
	(sys_call_t)sys_read_file,
	(sys_call_t)sys_fb_blit,
	(sys_call_t)sys_poll_scancode,
	(sys_call_t)sys_ticks_ms,
	(sys_call_t)sys_pcspk,
	(sys_call_t)sys_opl_io,
	(sys_call_t)sys_readdir,
};
