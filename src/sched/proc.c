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

void proc_exit(int retvalue)
{
	current_proc->retvalue = retvalue;
	__proc_kill(current_proc);
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
	struct proc *proc = kmalloc(sizeof(*proc));
	*proc = proc_inst(PROC_READY, ptr, stack_size);

	arch_proc_init(&proc->ctx);

	proc->stack_base = usr_kmalloc(stack_size);
	proc->ctx.intf.hw.rsp = (virt_array)(((uintptr_t)proc->stack_base + stack_size) & ~0xFULL);
	proc->ctx.intf.hw.rflags = (1 << 9) | 0b10;
	// Considering real programs, they will be loaded from storage, so this
	// is just a dummy implementation anyway, so
	// TODO! substitute this.

	proc->ctx.intf.hw.rip = usr_vmalloc(code_size);
	memcpy(proc->ctx.intf.hw.rip, ptr, code_size);

	// Add it into hash table
	hash_table_id_insert(proc);

	return proc;
}

extern struct proc *proc_sched();
struct proc *proc_int(struct int_frame *intf)
{
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

	return current_proc;
}
