#include "syscall.h"
#include "sim.h"

static struct sim s_sim;
static uint32_t s_fb[SIMSO_FB_W * SIMSO_FB_H];

static void copy_str16(char dst[16], const char *src)
{
	int i = 0;
	for (; i < 15 && src[i]; i++) dst[i] = src[i];
	for (; i < 16; i++) dst[i] = '\0';
}

static void load_caso_base(struct sim *s, uint32_t alg)
{
	uint8_t *raw = (uint8_t *)s;
	for (size_t i = 0; i < sizeof(*s); i++) raw[i] = 0;

	s->alg = alg; s->quantum = 2; s->overhead = 1; s->disk_cost = 3; s->seed = 42; s->n = 3;

	copy_str16(s->tasks[0].name, "P1");
	s->tasks[0].arrival = 0; s->tasks[0].burst = 5; s->tasks[0].priority = 2; s->tasks[0].deadline = 8; s->tasks[0].num_pages = 6;

	copy_str16(s->tasks[1].name, "P2");
	s->tasks[1].arrival = 1; s->tasks[1].burst = 4; s->tasks[1].priority = 1; s->tasks[1].deadline = 12; s->tasks[1].num_pages = 4;

	copy_str16(s->tasks[2].name, "P3");
	s->tasks[2].arrival = 3; s->tasks[2].burst = 2; s->tasks[2].priority = 3; s->tasks[2].deadline = 20; s->tasks[2].num_pages = 3;
}

static int run_kernel_sim(uint32_t alg)
{
	load_caso_base(&s_sim, alg);
	if (sys_sim_start(&s_sim) < 0) return -1;
	int64_t r;
	while ((r = sys_sim_poll(&s_sim)) == 0)
		asm volatile("pause");
	return (r > 0) ? 0 : -1;
}

int simso_main(void)
{
	uint32_t alg = ALG_RR;
	if (run_kernel_sim(alg) != 0) return 1;

	uint32_t vis = s_sim.total_ticks;
	render_gantt(s_fb, SIMSO_FB_W, SIMSO_FB_H, &s_sim, vis);
	sys_fb_blit(s_fb, SIMSO_FB_W, SIMSO_FB_H);

	for (;;) {
		int32_t sc = sys_poll_scancode();
		if (sc <= 0) { asm volatile("pause"); continue; }
		if (sc & 0x80) continue;

		int dirty = 0;
		if (sc == 0x01 || sc == 0x10) return 0; /* ESC / Q */
		else if (sc >= 0x02 && sc <= 0x08) {
			alg = (uint32_t)(sc - 0x02);
			if (run_kernel_sim(alg) == 0) { vis = s_sim.total_ticks; dirty = 1; }
		} else if (sc == 0x39) { /* Space: step-by-step */
			vis = (vis >= s_sim.total_ticks) ? 1 : (vis + 1);
			dirty = 1;
		} else if (sc == 0x1C) { /* Enter: full Gantt */
			vis = s_sim.total_ticks;
			dirty = 1;
		}
		if (dirty) {
			render_gantt(s_fb, SIMSO_FB_W, SIMSO_FB_H, &s_sim, vis);
			sys_fb_blit(s_fb, SIMSO_FB_W, SIMSO_FB_H);
		}
	}
}
