#include "sched/sim/sim_engine.h"
#include "fs/fat32.h"
#include "util/memory.h"

enum sim_engine_state {
	SIM_STATE_IDLE    = 0,
	SIM_STATE_RUNNING = 1,
	SIM_STATE_DONE    = 2
};

static struct sim s_sim;
static volatile enum sim_engine_state s_state = SIM_STATE_IDLE;
static struct proc *s_owner = NULL;
static struct proc *s_worker_pool[SIM_MAX_TASKS] = {0};
static uint32_t s_worker_pool_count = 0;

/* Fallback 2-byte Ring 3 busy-loop (`jmp .`) if WORKER.BIN is not on disk. */
static const uint8_t s_fallback_worker_code[2] = { 0xEB, 0xFE };

static bool ensure_workers(uint32_t needed)
{
	if (needed > SIM_MAX_TASKS)
		return false;

	bool all_ready = (s_worker_pool_count >= needed);
	if (all_ready) {
		for (uint32_t i = 0; i < needed; i++) {
			if (!s_worker_pool[i] || s_worker_pool[i]->state == PROC_DEAD) {
				all_ready = false;
				break;
			}
		}
	}
	if (all_ready)
		return true;

	struct fat32_dir_entry *root = fat32_open_root_dir();
	struct file worker_file = {0};
	if (root)
		worker_file = fat32_read_file(root, "WORKER  BIN");

	const void *code_ptr = (worker_file.mptr && worker_file.size > 0)
	                           ? worker_file.mptr
	                           : (const void *)s_fallback_worker_code;
	size_t code_size = (worker_file.mptr && worker_file.size > 0)
	                       ? worker_file.size
	                       : sizeof(s_fallback_worker_code);

	for (uint32_t i = 0; i < needed; i++) {
		if (s_worker_pool[i] && s_worker_pool[i]->state != PROC_DEAD)
			continue;
		struct proc *w = proc_create_sized((virt_t)code_ptr, code_size, 4096, 4096);
		if (!w)
			break;
		w->state = PROC_BLOCK;
		s_worker_pool[i] = w;
		if (i + 1 > s_worker_pool_count)
			s_worker_pool_count = i + 1;
	}

	if (worker_file.mptr)
		fat32_close_file(worker_file);

	return s_worker_pool_count >= needed;
}

int64_t sim_engine_start(const struct sim *in_sim, struct proc *owner)
{
	if (!in_sim || !owner || s_state == SIM_STATE_RUNNING)
		return -1;
	if (!ensure_workers(in_sim->n))
		return -1;

	memcpy(&s_sim, in_sim, sizeof(struct sim));
	if (!sim_init(&s_sim))
		return -1;

	s_owner = owner;
	s_state = SIM_STATE_RUNNING;
	return 0;
}

int64_t sim_engine_poll(struct sim *out_sim)
{
	if (!out_sim)
		return -1;
	if (s_state == SIM_STATE_RUNNING)
		return 0;
	if (s_state == SIM_STATE_DONE) {
		memcpy(out_sim, &s_sim, sizeof(struct sim));
		s_state = SIM_STATE_IDLE;
		return 1;
	}
	return -1;
}

bool sim_engine_is_active(void)
{
	return s_state == SIM_STATE_RUNNING;
}

struct proc *sim_engine_tick(void)
{
	if (s_state != SIM_STATE_RUNNING)
		return s_owner;

	int active = -1;
	bool running = sim_step(&s_sim, &active);

	if (!running) {
		s_state = SIM_STATE_DONE;
		for (uint32_t i = 0; i < s_worker_pool_count; i++) {
			if (s_worker_pool[i])
				s_worker_pool[i]->state = PROC_BLOCK;
		}
		if (s_owner)
			s_owner->state = PROC_READY;
		return s_owner;
	}

	if (active >= 0 && (uint32_t)active < s_worker_pool_count && s_worker_pool[active]) {
		s_worker_pool[active]->sched = s_sim.tasks[active];
		return s_worker_pool[active];
	}
	return s_owner;
}

void sim_engine_on_proc_exit(struct proc *proc)
{
	if (s_state != SIM_STATE_RUNNING || !proc)
		return;
	for (uint32_t i = 0; i < s_sim.n && i < s_worker_pool_count; i++) {
		if (s_worker_pool[i] == proc) {
			s_worker_pool[i] = NULL;
			sim_notify_exit(&s_sim, i);
			return;
		}
	}
}
