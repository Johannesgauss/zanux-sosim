#include "sys/syscall.h"
#include "sys/io.h"
#include "sys/exec.h"
#include "arch/sys/syscall.h"
#include "arch/gdt_macros.h"

const sys_call_t syscall_table[SYSCALL_NUMBER] = {
	[SYS_EXIT]          = (sys_call_t)sys_exit,
	[SYS_WRITE]         = (sys_call_t)sys_write,
	[SYS_READ]          = (sys_call_t)sys_read,
	[SYS_EXEC]          = (sys_call_t)sys_exec,
	[SYS_ALLOC]         = (sys_call_t)sys_alloc,
	[SYS_READ_FILE]     = (sys_call_t)sys_read_file,
	[SYS_FB_BLIT]       = (sys_call_t)sys_fb_blit,
	[SYS_POLL_SCANCODE] = (sys_call_t)sys_poll_scancode,
	[SYS_TICKS_MS]      = (sys_call_t)sys_ticks_ms,
	[SYS_PCSPK]         = (sys_call_t)sys_pcspk,
	[SYS_OPL_IO]        = (sys_call_t)sys_opl_io,
	[SYS_READDIR]       = (sys_call_t)sys_readdir,
	[SYS_SIM_START]     = (sys_call_t)sys_sim_start,
	[SYS_SIM_POLL]      = (sys_call_t)sys_sim_poll,
	[SYS_AUDIO_WRITE]   = (sys_call_t)sys_audio_write,
	[SYS_GET_ARGS]      = (sys_call_t)sys_get_args,
	[SYS_WIN_BLIT]      = (sys_call_t)sys_win_blit,
	[SYS_WIN_READ]      = (sys_call_t)sys_win_read,
	[SYS_WRITE_FILE]    = (sys_call_t)sys_write_file,
};
