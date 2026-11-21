#include "sim.h"

/* Reference Round-Robin implementation using `user_data` as a FIFO enqueue timestamp. */
int pick_rr(struct task *t, uint32_t n, int cur, uint32_t now, uint32_t q)
{
	if (q == 0)
		q = 2;

	/* Assign monotonic sequence numbers to newly ready tasks (user_data == 0). */
	uint32_t max_seq = 0;
	for (uint32_t i = 0; i < n; i++) {
		if (t[i].user_data > max_seq)
			max_seq = t[i].user_data;
	}
	for (uint32_t i = 0; i < n; i++) {
		if ((t[i].state == TASK_READY || t[i].state == TASK_RUNNING) && t[i].user_data == 0)
			t[i].user_data = ++max_seq;
	}

	if (cur >= 0 && (uint32_t)cur < n && t[cur].state == TASK_RUNNING) {
		if (t[cur].slice_used < q)
			return cur;
		/* Quantum expired: move current task to the tail of the FIFO queue. */
		t[cur].user_data = ++max_seq;
	}

	(void)now;
	int best = -1;
	uint32_t best_seq = 0xFFFFFFFFU;
	for (uint32_t i = 0; i < n; i++) {
		if (t[i].state != TASK_READY && t[i].state != TASK_RUNNING)
			continue;
		if (t[i].user_data < best_seq) {
			best_seq = t[i].user_data;
			best = (int)i;
		}
	}
	return best;
}
