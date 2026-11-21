#include "sched/proc.h"
#include "sched/sim_engine.h"
#include "proc_util.h"

#define SCHED_MAX_PROCS 64

// struct proc *current_proc = the current process
struct proc *proc_sched()
{
	if (sim_engine_is_active())
		return sim_engine_tick();

	if (!current_proc)
		return hash_table_id_search(0);

	for (int step = 1; step <= SCHED_MAX_PROCS; step++) {
		ushort id = (ushort)((current_proc->id + step) % SCHED_MAX_PROCS);
		struct proc *p = hash_table_id_search(id);
		if (p && p->state != PROC_DEAD && p->state != PROC_BLOCK)
			return p;
	}

	if (current_proc->state != PROC_DEAD && current_proc->state != PROC_BLOCK)
		return current_proc;
	return NULL;
}
