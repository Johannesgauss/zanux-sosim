#include "sim.h"

int pick_sjf(struct task *t, uint32_t n, int cur, uint32_t now, uint32_t q)
{
	(void)now;
	(void)q;

	if (cur >= 0 && (uint32_t)cur < n && t[cur].state == TASK_RUNNING)
		return cur;

	// TODO: pick the TASK_READY task with the smallest `t[i].burst`.
	for (uint32_t i = 0; i < n; i++) {
		if (t[i].state == TASK_READY)
			return (int)i;
	}
	return -1;
}
