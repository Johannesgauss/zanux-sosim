#include "sched/sim/sim_engine.h"
#include "fs/fat32.h"
#include "mem/kmalloc.h"
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
static char s_worker_bin[SIM_MAX_TASKS][16] = {{0}};
static uint32_t s_worker_pool_count = 0;

/* Live pacing: when custom visual binaries (bin_name) are loaded, run each
 * discrete simulation tick for 24 IRQ0s (~240 ms at 100 Hz) so the user can
 * watch the real Ring 3 windows alternate live on screen! */
static uint32_t s_irq_pace = 1;
static uint32_t s_irq_subtick = 0;
static int s_active_slot = -1;

/* SIMSO process that already ran a simulation. Its first simulation is paced for the live
 * windows; every restart by the same process runs at full speed (see sim_engine_start). */
static struct proc *s_session_owner = NULL;

/* Per-slot mini-window framebuffers filled by real Ring 3 workers via SYS_WIN_BLIT. */
static uint32_t s_win_fb[SIM_WIN_SLOTS][SIM_WIN_W * SIM_WIN_H];
static uint8_t  s_win_valid[SIM_WIN_SLOTS];

/* Fallback 2-byte Ring 3 busy-loop (`jmp .`) if WORKER.BIN is not on disk. */
static const uint8_t s_fallback_worker_code[2] = { 0xEB, 0xFE };

static bool str16_eq(const char a[16], const char b[16])
{
	for (int i = 0; i < 16; i++) {
		if (a[i] != b[i])
			return false;
		if (a[i] == '\0')
			return true;
	}
	return true;
}

static void str16_copy(char dst[16], const char src[16])
{
	int i = 0;
	for (; i < 15 && src[i]; i++)
		dst[i] = src[i];
	for (; i < 16; i++)
		dst[i] = '\0';
}

static bool name_to_fat83(const char *in, char out[FAT32_FILENAME_SIZE])
{
	memset(out, ' ', FAT32_FILENAME_SIZE);
	size_t i = 0;
	while (*in && *in != '.' && i < 8) {
		char c = *in++;
		if (c >= 'a' && c <= 'z')
			c = (char)(c - 32);
		out[i++] = c;
	}
	if (i == 0)
		return false;
	if (*in == '.') {
		in++;
		size_t j = 0;
		while (*in && j < 3) {
			char c = *in++;
			if (c >= 'a' && c <= 'z')
				c = (char)(c - 32);
			out[8 + j++] = c;
		}
	}
	return *in == '\0';
}

static bool ensure_workers(const struct sim *in_sim, bool *has_custom_bins)
{
	uint32_t needed = in_sim->n;
	if (needed == 0 || needed > SIM_MAX_TASKS)
		return false;

	bool any_custom = false;
	for (uint32_t i = 0; i < needed; i++) {
		if (in_sim->tasks[i].bin_name[0] != '\0')
			any_custom = true;
	}
	if (has_custom_bins)
		*has_custom_bins = any_custom;

	struct fat32_dir_entry *root = fat32_open_root_dir();
	struct file default_file = {0};
	if (root)
		default_file = fat32_read_file(root, "WORKER  BIN");

	const void *def_ptr = (default_file.mptr && default_file.size > 0)
	                          ? default_file.mptr
	                          : (const void *)s_fallback_worker_code;
	size_t def_size = (default_file.mptr && default_file.size > 0)
	                      ? default_file.size
	                      : sizeof(s_fallback_worker_code);

	for (uint32_t i = 0; i < needed; i++) {
		const char *want_bin = in_sim->tasks[i].bin_name;
		bool reusable = (s_worker_pool[i] && s_worker_pool[i]->state != PROC_DEAD &&
		                 str16_eq(s_worker_bin[i], want_bin));
		if (reusable)
			continue;

		if (s_worker_pool[i] && s_worker_pool[i]->state != PROC_DEAD) {
			proc_kill(s_worker_pool[i]->id);
			s_worker_pool[i] = NULL;
			s_worker_bin[i][0] = '\0';
		}

		struct file custom_file = {0};
		const void *code_ptr = def_ptr;
		size_t code_size = def_size;
		size_t bss_size = 4096;
		size_t stack_size = 4096;

		if (root && want_bin[0] != '\0') {
			char fat_name[FAT32_FILENAME_SIZE];
			if (name_to_fat83(want_bin, fat_name)) {
				custom_file = fat32_read_file(root, fat_name);
				if (custom_file.mptr && custom_file.size > 0) {
					code_ptr = custom_file.mptr;
					code_size = custom_file.size;
					bss_size = 131072; /* 128 KB BSS for Ring 3 mini-window canvas */
					stack_size = 16384;
				}
			}
		}

		struct proc *w = proc_create_sized((virt_t)code_ptr, code_size, bss_size, stack_size);
		if (custom_file.mptr)
			fat32_close_file(custom_file);
		if (!w)
			break;

		w->state = PROC_BLOCK;
		s_worker_pool[i] = w;
		str16_copy(s_worker_bin[i], want_bin);
		if (i + 1 > s_worker_pool_count)
			s_worker_pool_count = i + 1;
	}

	if (default_file.mptr)
		fat32_close_file(default_file);
	if (root)
		kfree(root);

	for (uint32_t i = 0; i < needed; i++) {
		if (!s_worker_pool[i] || s_worker_pool[i]->state == PROC_DEAD)
			return false;
	}
	return true;
}

/* Stop the running simulation and park every worker, like the normal end of a run. */
static void abort_running(void)
{
	s_state = SIM_STATE_IDLE;
	s_active_slot = -1;
	for (uint32_t i = 0; i < s_worker_pool_count; i++) {
		if (s_worker_pool[i])
			s_worker_pool[i]->state = PROC_BLOCK;
	}
}

int64_t sim_engine_start(const struct sim *in_sim, struct proc *owner)
{
	if (!in_sim || !owner)
		return -1;

	/* A new start while one is running replaces it: this is how a key pressed in SIMSO
	 * cancels the live run. Only the process that started the run may do so. */
	if (s_state == SIM_STATE_RUNNING) {
		if (owner != s_owner)
			return -1;
		abort_running();
	}

	bool restart = (owner == s_session_owner);
	bool has_custom = false;
	if (!ensure_workers(in_sim, &has_custom))
		return -1;

	memcpy(&s_sim, in_sim, sizeof(struct sim));
	if (!sim_init(&s_sim))
		return -1;

	/* On a restart keep the last window frames, so the cards do not go blank. */
	for (uint32_t i = 0; !restart && i < SIM_WIN_SLOTS; i++) {
		s_win_valid[i] = 0;
		memset(s_win_fb[i], 0, sizeof(s_win_fb[i]));
	}

	s_irq_pace = (has_custom && !restart) ? 24U : 1U;
	s_session_owner = owner;
	s_irq_subtick = 0;
	s_active_slot = -1;
	s_owner = owner;
	s_state = SIM_STATE_RUNNING;
	return 0;
}

int64_t sim_engine_poll(struct sim *out_sim)
{
	if (!out_sim)
		return -1;
	if (s_state == SIM_STATE_RUNNING) {
		/* Live snapshot so SIMSO.BIN can render Gantt + Ring 3 windows while running! */
		memcpy(out_sim, &s_sim, sizeof(struct sim));
		return 0;
	}
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

	if (s_irq_subtick == 0) {
		int active = -1;
		bool running = sim_step(&s_sim, &active);
		s_active_slot = active;

		if (!running) {
			s_state = SIM_STATE_DONE;
			s_active_slot = -1;
			for (uint32_t i = 0; i < s_worker_pool_count; i++) {
				if (s_worker_pool[i])
					s_worker_pool[i]->state = PROC_BLOCK;
			}
			if (s_owner)
				s_owner->state = PROC_READY;
			return s_owner;
		}
	}

	uint32_t sub = s_irq_subtick;
	s_irq_subtick++;
	if (s_irq_subtick >= s_irq_pace)
		s_irq_subtick = 0;

	/* When live-paced, yield 1 IRQ0 every 40ms to s_owner so the UI refreshes at ~25 FPS. */
	if (s_irq_pace > 1 && (sub & 3U) == 3U)
		return s_owner;

	if (s_active_slot >= 0 && (uint32_t)s_active_slot < s_worker_pool_count && s_worker_pool[s_active_slot]) {
		s_worker_pool[s_active_slot]->sched = s_sim.tasks[s_active_slot];
		return s_worker_pool[s_active_slot];
	}
	return s_owner;
}

void sim_engine_on_proc_exit(struct proc *proc)
{
	if (proc && proc == s_session_owner)
		s_session_owner = NULL;
	if (proc && proc == s_owner) {
		/* SIMSO quit (e.g. Esc) while a run was in progress: stop it, so the scheduler
		 * never hands the CPU to a dead owner. */
		if (s_state == SIM_STATE_RUNNING)
			abort_running();
		s_owner = NULL;
	}
	if (s_state != SIM_STATE_RUNNING || !proc)
		return;
	for (uint32_t i = 0; i < s_sim.n && i < s_worker_pool_count; i++) {
		if (s_worker_pool[i] == proc) {
			s_worker_pool[i] = NULL;
			s_worker_bin[i][0] = '\0';
			sim_notify_exit(&s_sim, i);
			return;
		}
	}
}

int64_t sim_engine_win_blit(struct proc *caller, const uint32_t *buf, uint32_t w, uint32_t h)
{
	if (!caller || !buf || w == 0 || h == 0)
		return -1;

	int slot = -1;
	for (uint32_t i = 0; i < s_worker_pool_count && i < SIM_WIN_SLOTS; i++) {
		if (s_worker_pool[i] == caller) {
			slot = (int)i;
			break;
		}
	}
	if (slot < 0)
		return -1;

	uint32_t copy_w = (w < SIM_WIN_W) ? w : SIM_WIN_W;
	uint32_t copy_h = (h < SIM_WIN_H) ? h : SIM_WIN_H;
	uint32_t *dst = s_win_fb[slot];
	for (uint32_t y = 0; y < copy_h; y++)
		memcpy(dst + y * SIM_WIN_W, buf + y * w, copy_w * sizeof(uint32_t));

	s_win_valid[slot] = 1;
	return 0;
}

int64_t sim_engine_win_read(uint32_t slot, uint32_t *dst, uint32_t max_pixels)
{
	if (slot >= SIM_WIN_SLOTS || !dst)
		return -1;
	if (!s_win_valid[slot])
		return 0;

	uint32_t count = SIM_WIN_W * SIM_WIN_H;
	if (max_pixels < count)
		count = max_pixels;
	memcpy(dst, s_win_fb[slot], count * sizeof(uint32_t));
	return 1;
}
