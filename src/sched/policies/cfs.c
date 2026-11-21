#include "sim.h"

int pick_cfs(struct task *t, uint32_t n, int cur, uint32_t now, uint32_t q)
{
	(void)cur;
	(void)now;
	(void)q;

	/*
	 * TODO: CFS-Sim (pure integer math with CFS_SCALE = 1024):
	 *  - sim_core initializes `t[i].vruntime = now * CFS_SCALE` on arrival
	 *    and increments `t[i].vruntime += priority * CFS_SCALE` each tick
	 *    (you can also customize weight right here or in sim_core.c).
	 *  - Return the TASK_READY/TASK_RUNNING task with the smallest `t[i].vruntime`.
	 */
	for (uint32_t i = 0; i < n; i++) {
		if (t[i].state == TASK_READY || t[i].state == TASK_RUNNING)
			return (int)i;
	}
	return -1;
}
