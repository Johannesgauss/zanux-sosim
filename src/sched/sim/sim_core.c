#include "sim.h"
#include <stddef.h>

static int dispatch_policy(struct sim *s)
{
	switch (s->alg) {
	case ALG_FCFS:    return pick_fcfs   (s->tasks, s->n, s->cur, s->now, s->quantum);
	case ALG_SJF:     return pick_sjf    (s->tasks, s->n, s->cur, s->now, s->quantum);
	case ALG_RR:      return pick_rr     (s->tasks, s->n, s->cur, s->now, s->quantum);
	case ALG_PRIO:    return pick_prio   (s->tasks, s->n, s->cur, s->now, s->quantum);
	case ALG_EDF:     return pick_edf    (s->tasks, s->n, s->cur, s->now, s->quantum);
	case ALG_CFS:     return pick_cfs    (s->tasks, s->n, s->cur, s->now, s->quantum);
	case ALG_AUTORAL: return pick_autoral(s->tasks, s->n, s->cur, s->now, s->quantum);
	default:          return -1;
	}
}

bool sim_init(struct sim *s)
{
	if (!s || s->n == 0 || s->n > SIM_MAX_TASKS || s->alg >= ALG_COUNT)
		return false;

	s->total_ticks = 0;
	s->idle_ticks = 0;
	s->overhead_ticks = 0;
	s->ctx_switches = 0;
	s->total_preemptions = 0;
	s->now = 0;
	s->done_count = 0;
	s->overhead_left = 0;
	s->finished = 0;
	s->cur = -1;
	s->last = -1;
	s->pending_next = -1;

	for (uint32_t i = 0; i < SIM_MAX_TICKS; i++) {
		s->tl_kind[i] = TICK_IDLE;
		s->tl_task[i] = -1;
	}

	for (uint32_t i = 0; i < s->n; i++) {
		struct task *t = &s->tasks[i];
		t->remaining   = t->burst;
		t->vruntime    = 0;
		t->slice_used  = 0;
		t->state       = TASK_PENDING;
		t->user_data   = 0;
		t->start       = 0xFFFFFFFFU;
		t->finish      = 0;
		t->wait        = 0;
		t->turnaround  = 0;
		t->preemptions = 0;
		t->page_faults = 0;
		t->ok          = 0;
	}
	return true;
}

static void record_tick(struct sim *s, enum tick_kind kind, int idx)
{
	if (s->now < SIM_MAX_TICKS) {
		s->tl_kind[s->now] = (uint8_t)kind;
		s->tl_task[s->now] = (int8_t)idx;
	}
}

bool sim_step(struct sim *s, int *active_task)
{
	if (active_task)
		*active_task = -1;
	if (!s || s->finished)
		return false;

	if (s->done_count >= s->n || s->now >= SIM_MAX_TICKS) {
		s->total_ticks = s->now;
		s->finished = 1;
		return false;
	}

	/* 1. Admit arriving tasks (`arrival <= now`). */
	for (uint32_t i = 0; i < s->n; i++) {
		if (s->tasks[i].state == TASK_PENDING && s->tasks[i].arrival <= s->now) {
			s->tasks[i].state = TASK_READY;
			s->tasks[i].vruntime = s->now * CFS_SCALE;
		}
	}

	/* 2. Consume context-switch overhead ticks if active. */
	if (s->overhead_left > 0) {
		record_tick(s, TICK_OVERHEAD, -1);
		s->overhead_ticks++;
		s->overhead_left--;
		s->now++;
		if (s->overhead_left == 0) {
			s->cur = s->pending_next;
			s->pending_next = -1;
			if (s->cur >= 0) {
				s->tasks[s->cur].state = TASK_RUNNING;
				s->tasks[s->cur].slice_used = 0;
			}
		}
		return true;
	}

	/* 3. Ask the selected algorithm who runs on tick `s->now`. */
	int next = dispatch_policy(s);
	if (next < 0 || (uint32_t)next >= s->n ||
	    s->tasks[next].state == TASK_DONE || s->tasks[next].state == TASK_PENDING) {
		if (s->cur >= 0 && s->tasks[s->cur].state == TASK_RUNNING) {
			s->tasks[s->cur].state = TASK_READY;
			s->tasks[s->cur].slice_used = 0;
		}
		s->cur = -1;
		record_tick(s, TICK_IDLE, -1);
		s->idle_ticks++;
		s->now++;
		return true;
	}

	/* 4. Context switch detection & overhead. */
	if (s->last >= 0 && next != s->last) {
		s->ctx_switches++;
		if (s->cur == s->last && s->tasks[s->last].state == TASK_RUNNING) {
			s->tasks[s->last].state = TASK_READY;
			s->tasks[s->last].slice_used = 0;
			s->tasks[s->last].preemptions++;
			s->total_preemptions++;
		}
		s->last = next;

		if (s->overhead > 0) {
			s->cur = -1;
			s->pending_next = next;
			s->overhead_left = s->overhead - 1;
			record_tick(s, TICK_OVERHEAD, -1);
			s->overhead_ticks++;
			s->now++;
			if (s->overhead_left == 0) {
				s->cur = s->pending_next;
				s->pending_next = -1;
				s->tasks[s->cur].state = TASK_RUNNING;
				s->tasks[s->cur].slice_used = 0;
			}
			return true;
		}
		s->cur = next;
		s->tasks[next].slice_used = 0;
	} else if (s->cur != next) {
		s->cur = next;
		s->last = next;
		s->tasks[next].slice_used = 0;
	}

	/* 5. Execute 1 tick of `s->cur`. */
	struct task *t = &s->tasks[s->cur];
	t->state = TASK_RUNNING;
	if (t->start == 0xFFFFFFFFU)
		t->start = s->now;

	record_tick(s, (s->now >= t->deadline) ? TICK_MISS : TICK_EXEC, s->cur);
	if (active_task)
		*active_task = s->cur;

	if (t->burst > 0)
		t->remaining--;

	t->slice_used++;
	uint32_t prio = (t->priority > 0) ? t->priority : 1;
	t->vruntime += prio * CFS_SCALE;
	s->now++;

	if (t->burst > 0 && t->remaining == 0) {
		t->state = TASK_DONE;
		t->finish = s->now;
		t->turnaround = t->finish - t->arrival;
		t->wait = t->turnaround - t->burst;
		t->ok = (t->finish <= t->deadline) ? 1U : 0U;
		s->done_count++;
		s->cur = -1;
	}

	if (s->done_count >= s->n || s->now >= SIM_MAX_TICKS) {
		s->total_ticks = s->now;
		s->finished = 1;
		return false;
	}
	return true;
}

void sim_notify_exit(struct sim *s, uint32_t idx)
{
	if (!s || s->finished || idx >= s->n)
		return;
	struct task *t = &s->tasks[idx];
	if (t->state == TASK_DONE)
		return;
	t->state = TASK_DONE;
	t->remaining = 0;
	t->finish = s->now;
	if (t->start == 0xFFFFFFFFU)
		t->start = s->now;
	if (t->burst == 0) {
		uint32_t ran = 0;
		uint32_t lim = (s->now < SIM_MAX_TICKS) ? s->now : SIM_MAX_TICKS;
		for (uint32_t k = 0; k < lim; k++) {
			if (s->tl_task[k] == (int8_t)idx)
				ran++;
		}
		t->burst = ran;
	}
	t->turnaround = (t->finish >= t->arrival) ? (t->finish - t->arrival) : 0;
	t->wait = (t->turnaround >= t->burst) ? (t->turnaround - t->burst) : 0;
	t->ok = (t->finish <= t->deadline) ? 1U : 0U;
	s->done_count++;
	if (s->cur == (int32_t)idx)
		s->cur = -1;
}
