#include "sim.h"

int pick_fcfs(struct task *t, uint32_t n, int cur, uint32_t now, uint32_t q)
{
	(void)now;
	(void)q;

	if (cur >= 0 && (uint32_t)cur < n && t[cur].state == TASK_RUNNING)
		return cur;

	int best = -1;
	uint32_t best_arr = 0xFFFFFFFFU;
	for (uint32_t i = 0; i < n; i++) {
		if (t[i].state == TASK_READY && t[i].arrival < best_arr) {
			best_arr = t[i].arrival;
			best = (int)i;
		}
	}
	return best;
}
