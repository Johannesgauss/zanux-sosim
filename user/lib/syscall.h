#ifndef USER_LIB_SYSCALL_H
#define USER_LIB_SYSCALL_H

#include <stddef.h>
#include <stdint.h>

#define SYS_EXIT          0
#define SYS_WRITE         1
#define SYS_READ          2
#define SYS_EXEC          3
#define SYS_ALLOC         4
#define SYS_READ_FILE     5
#define SYS_FB_BLIT       6
#define SYS_POLL_SCANCODE 7
#define SYS_TICKS_MS      8
#define SYS_PCSPK         9
#define SYS_OPL_IO        10
#define SYS_READDIR       11
#define SYS_SIM_START     12
#define SYS_SIM_POLL      13
#define SYS_AUDIO_WRITE   14
#define SYS_GET_ARGS      15
#define SYS_WIN_BLIT      16
#define SYS_WIN_READ      17
#define SYS_WRITE_FILE    18

static inline int64_t syscall0(uint64_t num)
{
	int64_t ret;
	asm volatile(
		"syscall"
		: "=a"(ret)
		: "a"(num)
		: "rcx", "r11", "memory"
	);
	return ret;
}

static inline int64_t syscall1(uint64_t num, uint64_t a1)
{
	int64_t ret;
	asm volatile(
		"syscall"
		: "=a"(ret)
		: "a"(num), "D"(a1)
		: "rcx", "r11", "memory"
	);
	return ret;
}

static inline int64_t syscall3(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3)
{
	int64_t ret;
	asm volatile(
		"syscall"
		: "=a"(ret)
		: "a"(num), "D"(a1), "S"(a2), "d"(a3)
		: "rcx", "r11", "memory"
	);
	return ret;
}

static inline void sys_exit(int code)
{
	syscall1(SYS_EXIT, (uint64_t)code);
	for (;;) {}
}

static inline int64_t sys_write(int fd, const void *buf, size_t len)
{
	return syscall3(SYS_WRITE, (uint64_t)fd, (uint64_t)buf, (uint64_t)len);
}

static inline int64_t sys_read(int fd, void *buf, size_t len)
{
	return syscall3(SYS_READ, (uint64_t)fd, (uint64_t)buf, (uint64_t)len);
}

static inline int64_t sys_exec(const char *name)
{
	return syscall1(SYS_EXEC, (uint64_t)name);
}

static inline void *sys_alloc(size_t size)
{
	return (void *)(uintptr_t)syscall1(SYS_ALLOC, (uint64_t)size);
}

static inline int64_t sys_read_file(const char *name, void *dst, size_t max_len)
{
	return syscall3(SYS_READ_FILE, (uint64_t)name, (uint64_t)dst, (uint64_t)max_len);
}

static inline int64_t sys_fb_blit(const uint32_t *buf, uint32_t w, uint32_t h)
{
	return syscall3(SYS_FB_BLIT, (uint64_t)buf, (uint64_t)w, (uint64_t)h);
}

static inline int32_t sys_poll_scancode(void)
{
	return (int32_t)syscall0(SYS_POLL_SCANCODE);
}

static inline uint32_t sys_ticks_ms(void)
{
	return (uint32_t)syscall0(SYS_TICKS_MS);
}

static inline void sys_pcspk(uint16_t divisor)
{
	syscall1(SYS_PCSPK, (uint64_t)divisor);
}

static inline uint8_t sys_opl_read(uint16_t port)
{
	return (uint8_t)syscall3(SYS_OPL_IO, (uint64_t)port, 0, 0);
}

static inline void sys_opl_write(uint16_t port, uint8_t val)
{
	syscall3(SYS_OPL_IO, (uint64_t)port, (uint64_t)val, 1);
}

static inline int64_t sys_readdir(size_t index, char name_out[16], uint32_t *size_out)
{
	return syscall3(SYS_READDIR, (uint64_t)index, (uint64_t)name_out, (uint64_t)size_out);
}

/* Flag for sys_sim_start(): pace each tick so the live Ring 3 windows can be watched.
 * Without it the simulation runs at full speed. Keep in sync with sim_engine.h. */
#define SIM_START_LIVE 1ULL

static inline int64_t sys_sim_start(const void *cfg, uint64_t flags)
{
	return syscall3(SYS_SIM_START, (uint64_t)cfg, flags, 0);
}

static inline int64_t sys_sim_poll(void *out_trace)
{
	return syscall1(SYS_SIM_POLL, (uint64_t)out_trace);
}

static inline int64_t sys_audio_write(const uint8_t *pcm, size_t len)
{
	return syscall3(SYS_AUDIO_WRITE, (uint64_t)pcm, (uint64_t)len, 0);
}

static inline int64_t sys_get_args(char *dst, size_t max_len)
{
	return syscall3(SYS_GET_ARGS, (uint64_t)dst, (uint64_t)max_len, 0);
}

static inline int64_t sys_win_blit(const uint32_t *buf, uint32_t w, uint32_t h)
{
	return syscall3(SYS_WIN_BLIT, (uint64_t)buf, (uint64_t)w, (uint64_t)h);
}

static inline int64_t sys_win_read(uint32_t slot, uint32_t *dst, uint32_t max_pixels)
{
	return syscall3(SYS_WIN_READ, (uint64_t)slot, (uint64_t)dst, (uint64_t)max_pixels);
}

static inline int64_t sys_write_file(const char *name, const void *src, size_t len)
{
	return syscall3(SYS_WRITE_FILE, (uint64_t)name, (uint64_t)src, (uint64_t)len);
}

#endif // USER_LIB_SYSCALL_H
