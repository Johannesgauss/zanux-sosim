#include "sim.h"

int pick_edf(struct task *t, uint32_t n, int cur, uint32_t now, uint32_t q)
{
	(void)cur;
	(void)now;
	(void)q;

	// TODO: preemptive! Pick TASK_READY/TASK_RUNNING with smallest `t[i].deadline`.
	for (uint32_t i = 0; i < n; i++) {
		if (t[i].state == TASK_READY || t[i].state == TASK_RUNNING)
			return (int)i;
	}
	return -1;
}
