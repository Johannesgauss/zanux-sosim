#include "mem/kmalloc.h"
#include "util/alias.h"
#include "proc.h"
#include "test/test.h"
#include "proc_util.h"
//#include "drivers/gfx/print_text.h"

struct proc *proc_hash_table[PROC_HASH_TABLE_SIZE] = {0};
ushort id_count = 0;
struct proc *current_proc = NULL;

void proc_tmp_setup()
{
	current_proc = proc_create(&foo, 5000, 5000);
	proc_create(&fizz, 5000, 5000);
}

struct proc proc_inst(state_t state, virt_t ptr, size_t stack_size)
{
	(void)ptr;
	(void)stack_size;

	struct proc proc;

	proc.id = id_count;
	id_count++;

	proc.state = state;

	proc.ctx.intf.gp = (struct gp_regs) {0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0};

	return proc;
}

static void __proc_kill(struct proc *proc)
{
	proc->state = PROC_DEAD;
	vfree(proc->stack_base);
}

void proc_kill(ushort id)
{
	struct proc *proc = hash_table_id_search(id);
	__proc_kill(proc);
}

extern void kb_drain(void);

void proc_exit(int retvalue)
{
	current_proc->retvalue = retvalue;
	__proc_kill(current_proc);
	struct proc *shell = hash_table_id_search(0);
	if (shell && shell != current_proc && shell->state == PROC_BLOCK) {
		// Drop keystrokes typed into the child (e.g. DOOM) so the shell
		// doesn't replay them as commands.
		kb_drain();
		shell->state = PROC_READY;
	}
}

void proc_suspend(ushort id)
{
	struct proc *proc = hash_table_id_search(id);
	proc->state = PROC_READY;
}

void proc_switch(struct int_frame *intf, ushort id_next)
{
	if (in_user_mode(intf)) {
		current_proc->ctx.intf = *intf;
	}

	current_proc->state = PROC_READY;

	current_proc = hash_table_id_search(id_next);
	current_proc->state = PROC_EXEC;
}

struct proc *proc_create(virt_t ptr, size_t code_size, size_t stack_size)
{
	virt_array saved_pml4 = g_pml4;
	phy_t saved_cr3;
	asm volatile ("mov %%cr3, %0;" : "=r"(saved_cr3));

	struct proc *proc = kmalloc(sizeof(*proc));
	*proc = proc_inst(PROC_READY, ptr, stack_size);

	arch_proc_init(&proc->ctx);

	proc->stack_base = usr_kmalloc(stack_size);
	proc->ctx.intf.hw.rsp = (virt_array)(((uintptr_t)proc->stack_base + stack_size) & ~0xFULL);
	proc->ctx.intf.hw.rflags = (1 << 9) | 0b10;
	// Considering real programs, they will be loaded from storage, so this
	// is just a dummy implementation anyway, so
	// TODO! substitute this.
	// Map flat binary at fixed USER_CODE_BASE (0x400000) in this process's cr3,
	// plus 8 MiB of zero-initialized pages for .bss.
	const uintptr_t user_code_base = 0x0000000000400000ULL;
	size_t map_size = code_size + (8 * 1024 * 1024);
	for (size_t off = 0; off < map_size; off += PAGE_SIZE) {
		vmm_map((virt_t)(user_code_base + off));
	}
	proc->ctx.intf.hw.rip = (virt_t)user_code_base;
	memcpy(proc->ctx.intf.hw.rip, ptr, code_size);

	g_pml4 = saved_pml4;
	asm volatile ("mov %0, %%cr3;" :: "r"(saved_cr3) : "memory");

	// Add it into hash table
	hash_table_id_insert(proc);

	return proc;
}

volatile uint64_t g_timer_ticks = 0;

extern struct proc *proc_sched();
struct proc *proc_int(struct int_frame *intf)
{
	g_timer_ticks++;
	// TODO: I don't think this is the best solution!
	if (current_proc && in_user_mode(intf)) {
		current_proc->ctx.intf = *intf;
		if (current_proc->state == PROC_EXEC)
			current_proc->state = PROC_READY;
	}

	struct proc *next = proc_sched();

	if (!next) {
		while (1) {
			asm volatile ("hlt");
		}
	}
	next->state = PROC_EXEC;
	current_proc = next;
	g_pml4 = phy_to_virt(next->ctx.cr3);

	return current_proc;
}
