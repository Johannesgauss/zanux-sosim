#include "sim.h"
#include <stdio.h>
#include <string.h>

static const char *s_alg_names[ALG_COUNT] = {
	"FCFS", "SJF", "RR", "PRIO", "EDF", "CFS", "AUTORAL"
};

static struct sim make_caso_base(enum alg_id alg)
{
	struct sim s;
	memset(&s, 0, sizeof(s));
	s.alg = (uint32_t)alg; s.quantum = 2; s.overhead = 1; s.disk_cost = 3; s.seed = 42; s.n = 3;

	strcpy(s.tasks[0].name, "P1");
	s.tasks[0].arrival = 0; s.tasks[0].burst = 5; s.tasks[0].priority = 2; s.tasks[0].deadline = 8; s.tasks[0].num_pages = 6;
	strcpy(s.tasks[1].name, "P2");
	s.tasks[1].arrival = 1; s.tasks[1].burst = 4; s.tasks[1].priority = 1; s.tasks[1].deadline = 12; s.tasks[1].num_pages = 4;
	strcpy(s.tasks[2].name, "P3");
	s.tasks[2].arrival = 3; s.tasks[2].burst = 2; s.tasks[2].priority = 3; s.tasks[2].deadline = 20; s.tasks[2].num_pages = 3;
	return s;
}

static struct sim make_caso_idle(enum alg_id alg)
{
	struct sim s;
	memset(&s, 0, sizeof(s));
	s.alg = (uint32_t)alg; s.quantum = 2; s.overhead = 1; s.seed = 42; s.n = 2;

	strcpy(s.tasks[0].name, "P1");
	s.tasks[0].arrival = 0; s.tasks[0].burst = 3; s.tasks[0].priority = 1; s.tasks[0].deadline = 5;
	strcpy(s.tasks[1].name, "P2");
	s.tasks[1].arrival = 7; s.tasks[1].burst = 3; s.tasks[1].priority = 2; s.tasks[1].deadline = 12;
	return s;
}

static void run_and_print(const char *label, struct sim s)
{
	if (!sim_init(&s)) return;
	while (sim_step(&s, NULL)) {}

	printf("\n=== %s | %s ===\nGantt: ", label, s_alg_names[s.alg]);
	for (uint32_t t = 0; t < s.total_ticks && t < SIM_MAX_TICKS; t++) {
		if (s.tl_kind[t] == TICK_OVERHEAD) printf("[OV]");
		else if (s.tl_kind[t] == TICK_IDLE) printf("[..]");
		else if (s.tl_kind[t] == TICK_MISS) printf("[%s!]", s.tasks[s.tl_task[t]].name);
		else printf("[%s]", s.tasks[s.tl_task[t]].name);
	}
	printf("\n%-4s %4s %4s %4s %4s %5s %5s %5s %5s %4s\n",
	       "ID", "Arr", "Bur", "Pri", "Ddl", "Start", "End", "Wait", "Turn", "OK?");
	double sw = 0, st = 0;
	for (uint32_t i = 0; i < s.n; i++) {
		struct task *r = &s.tasks[i];
		sw += r->wait; st += r->turnaround;
		printf("%-4s %4u %4u %4u %4u %5u %5u %5u %5u %4s\n",
		       r->name, r->arrival, r->burst, r->priority, r->deadline,
		       r->start, r->finish, r->wait, r->turnaround, r->ok ? "YES" : "NO");
	}
	printf("Avg Wait: %.2f | Avg Turn: %.2f | Thr: %.3f | Idle+OV: %.1f%% | CtxSw: %u | Preempt: %u\n",
	       sw / s.n, st / s.n, (double)s.n / s.total_ticks,
	       100.0 * (s.idle_ticks + s.overhead_ticks) / s.total_ticks,
	       s.ctx_switches, s.total_preemptions);
}

int main(void)
{
	for (int a = 0; a < ALG_COUNT; a++)
		run_and_print("Caso Base", make_caso_base((enum alg_id)a));
	run_and_print("Caso Ociosidade", make_caso_idle(ALG_RR));
	return 0;
}
