#include "sim.h"

/*
 * Autoral policy: "dynamic score".
 *
 * Every tick, each runnable task gets an integer score; the highest score runs.
 *   score = W_URG * urgency + W_PRIO * prio + W_AGE * age (+ W_STICK if running)
 *
 *  - urgency: how close the task is to missing its deadline (small slack = high).
 *  - prio:    static priority (1 = most important = highest value).
 *  - age:     ticks spent waiting so far, so no task starves.
 *  - stick:   small bonus for the task already on the CPU, to avoid paying the
 *             context-switch overhead for tiny score differences.
 *
 * Kernel code (Ring 0): integers only, no malloc, no stdio.
 */

#define URG_MAX   32 /* slack (in ticks) at or above which a task is "not urgent" */
#define PRIO_MAX   8 /* priorities at or above this value count as the lowest */

#define W_URG      4
#define W_PRIO     3
#define W_AGE      1
#define W_STICK   32

int pick_autoral(struct task *t, uint32_t n, int cur, uint32_t now, uint32_t q)
{
	(void)q;

	int best = -1;
	int32_t best_score = 0;

	for (uint32_t i = 0; i < n; i++) {
		if (t[i].state != TASK_READY && t[i].state != TASK_RUNNING)
			continue;

		/* Urgency: slack = ticks left until the deadline minus the work still needed.
		 * slack < 0 means the deadline is already lost, so the task gets no urgency
		 * (do not burn CPU on a lost cause); aging will still bring it back later. */
		int32_t slack = (int32_t)t[i].deadline - (int32_t)now - (int32_t)t[i].remaining;
		int32_t urgency = 0;
		if (slack >= 0)
			urgency = URG_MAX - ((slack < URG_MAX) ? slack : URG_MAX);

		/* Priority: 1 (highest) maps to PRIO_MAX - 1, and PRIO_MAX or more maps to 0. */
		uint32_t p = (t[i].priority < PRIO_MAX) ? t[i].priority : PRIO_MAX;
		int32_t prio = (int32_t)(PRIO_MAX - p);

		/* Age: time since arrival minus the time already spent running. */
		uint32_t executed = (t[i].burst > t[i].remaining) ? (t[i].burst - t[i].remaining) : 0;
		int32_t age = 0;
		if (now > t[i].arrival + executed)
			age = (int32_t)(now - t[i].arrival - executed);

		int32_t score = W_URG * urgency + W_PRIO * prio + W_AGE * age;
		if ((int)i == cur)
			score += W_STICK;

		/* Strict ">" keeps the lowest index on ties, so the result is deterministic. */
		if (best < 0 || score > best_score) {
			best = (int)i;
			best_score = score;
		}
	}
	return best;
}
