#include "sim.h"

int pick_autoral(struct task *t, uint32_t n, int cur, uint32_t now, uint32_t q)
{
	(void)cur;
	(void)now;
	(void)q;

	// TODO: implement your custom / AI-inspired scheduling policy.
	for (uint32_t i = 0; i < n; i++) {
		if (t[i].state == TASK_READY || t[i].state == TASK_RUNNING)
			return (int)i;
	}
	return -1;
}
