#include "syscall.h"
#include "sim.h"
#include "simso_json.h"
#include <zgfx.h>

#define LIVE_WIN_W     144U
#define LIVE_WIN_H     72U
#define LIVE_WIN_SLOTS 4U

static struct sim s_base_cfg;
static struct sim s_sim;
static uint32_t s_fb[SIMSO_FB_W * SIMSO_FB_H];
static uint32_t s_win_tmp[LIVE_WIN_W * LIVE_WIN_H];
static char s_json_buf[8192];

static void copy_str16(char dst[16], const char *src)
{
	int i = 0;
	for (; i < 15 && src[i]; i++) dst[i] = src[i];
	for (; i < 16; i++) dst[i] = '\0';
}

static void copy_sim(struct sim *dst, const struct sim *src)
{
	const uint8_t *s = (const uint8_t *)src;
	uint8_t *d = (uint8_t *)dst;
	for (size_t i = 0; i < sizeof(struct sim); i++) d[i] = s[i];
}

static bool streq_ci(const char *a, const char *b)
{
	while (*a && *b) {
		char ca = *a++, cb = *b++;
		if (ca >= 'a' && ca <= 'z') ca = (char)(ca - 32);
		if (cb >= 'a' && cb <= 'z') cb = (char)(cb - 32);
		if (ca != cb) return false;
	}
	return *a == '\0' && *b == '\0';
}

static uint32_t parse_u32(const char *s, uint32_t fallback)
{
	if (!s || *s < '0' || *s > '9') return fallback;
	uint32_t v = 0;
	while (*s >= '0' && *s <= '9') {
		v = v * 10U + (uint32_t)(*s - '0');
		s++;
	}
	return v;
}

static int parse_alg(const char *s)
{
	if (!s || !s[0]) return -1;
	if (s[0] >= '0' && s[0] <= '6' && s[1] == '\0')
		return s[0] - '0';
	if (streq_ci(s, "FCFS") || streq_ci(s, "FIFO")) return ALG_FCFS;
	if (streq_ci(s, "SJF")) return ALG_SJF;
	if (streq_ci(s, "RR") || streq_ci(s, "ROUND-ROBIN")) return ALG_RR;
	if (streq_ci(s, "PRIO") || streq_ci(s, "PRIORIDADE") || streq_ci(s, "PRIORIDADES")) return ALG_PRIO;
	if (streq_ci(s, "EDF")) return ALG_EDF;
	if (streq_ci(s, "CFS") || streq_ci(s, "CFS-SIM")) return ALG_CFS;
	if (streq_ci(s, "AUTORAL") || streq_ci(s, "IA")) return ALG_AUTORAL;
	return -1;
}

static void load_caso_base(struct sim *s, uint32_t alg)
{
	uint8_t *raw = (uint8_t *)s;
	for (size_t i = 0; i < sizeof(*s); i++) raw[i] = 0;

	s->alg = alg; s->quantum = 2; s->overhead = 1; s->disk_cost = 3; s->seed = 42; s->n = 3;

	copy_str16(s->tasks[0].name, "P1");
	copy_str16(s->tasks[0].bin_name, "SORT.BIN");
	s->tasks[0].arrival = 0; s->tasks[0].burst = 5; s->tasks[0].priority = 2; s->tasks[0].deadline = 8; s->tasks[0].num_pages = 6;

	copy_str16(s->tasks[1].name, "P2");
	copy_str16(s->tasks[1].bin_name, "LIFE.BIN");
	s->tasks[1].arrival = 1; s->tasks[1].burst = 4; s->tasks[1].priority = 1; s->tasks[1].deadline = 12; s->tasks[1].num_pages = 4;

	copy_str16(s->tasks[2].name, "P3");
	copy_str16(s->tasks[2].bin_name, "PLASMA.BIN");
	s->tasks[2].arrival = 3; s->tasks[2].burst = 2; s->tasks[2].priority = 3; s->tasks[2].deadline = 20; s->tasks[2].num_pages = 3;
}

static bool load_json_file(const char *path, struct sim *out)
{
	int64_t n = sys_read_file(path, s_json_buf, sizeof(s_json_buf) - 1);
	if (n <= 0)
		return false;
	s_json_buf[n] = '\0';
	return simso_parse_json(s_json_buf, (size_t)n, out);
}

static void composite_live_windows(uint32_t *fb, const struct sim *s, uint32_t vis)
{
	uint32_t count = (s->n < LIVE_WIN_SLOTS) ? s->n : LIVE_WIN_SLOTS;
	if (count == 0)
		return;

	struct zcanvas cv = { fb, SIMSO_FB_W, SIMSO_FB_H };
	uint32_t card_w = LIVE_WIN_W + 4U;
	uint32_t card_h = LIVE_WIN_H + 18U;
	uint32_t gap = (count > 1U) ? ((SIMSO_FB_W - 32U - count * card_w) / (count - 1U)) : 0U;
	if (gap > 20U) gap = 20U;
	uint32_t total_w = count * card_w + (count - 1U) * gap;
	uint32_t start_x = (SIMSO_FB_W > total_w) ? ((SIMSO_FB_W - total_w) / 2U) : 16U;
	uint32_t y0 = 46U;

	int active_idx = -1;
	uint8_t active_kind = TICK_IDLE;
	if (vis > 0 && vis <= SIM_MAX_TICKS) {
		active_kind = s->tl_kind[vis - 1U];
		active_idx = s->tl_task[vis - 1U];
	}

	for (uint32_t i = 0; i < count; i++) {
		uint32_t x0 = start_x + i * (card_w + gap);
		const struct task *t = &s->tasks[i];

		bool is_running = (!s->finished && (int)i == active_idx &&
		                   (active_kind == TICK_EXEC || active_kind == TICK_MISS));
		bool is_ov = (!s->finished && active_kind == TICK_OVERHEAD);

		const char *badge = "WAIT";
		uint32_t badge_col = COL_GRID;
		uint32_t title_bg = 0x00242E3DU;

		if (is_running) {
			badge = (active_kind == TICK_MISS) ? "MISS" : "RUN";
			badge_col = (active_kind == TICK_MISS) ? COL_MISS : COL_EXEC;
			title_bg = (active_kind == TICK_MISS) ? 0x00444B54U : 0x001E5631U;
		} else if (is_ov) {
			badge = "OV";
			badge_col = COL_OVERHEAD;
		} else if (t->state == TASK_DONE) {
			badge = t->ok ? "OK" : "MISS";
			badge_col = t->ok ? COL_IO : COL_MISS;
		} else if (t->state == TASK_PENDING) {
			badge = "PEND";
			badge_col = COL_IDLE;
		}

		char title[24];
		uint32_t p = 0;
		for (uint32_t k = 0; t->name[k] && p < 6; k++) title[p++] = t->name[k];
		if (t->bin_name[0]) {
			title[p++] = ' ';
			for (uint32_t k = 0; t->bin_name[k] && t->bin_name[k] != '.' && p < 14; k++)
				title[p++] = t->bin_name[k];
		}
		title[p] = '\0';

		uint32_t border_col = is_running ? badge_col : (is_ov ? COL_OVERHEAD : COL_GRID);
		zgfx_window(&cv, x0, y0, card_w, card_h, title, badge, badge_col,
		            COL_PANEL, title_bg, border_col);

		if (sys_win_read(i, s_win_tmp, LIVE_WIN_W * LIVE_WIN_H) > 0) {
			for (uint32_t py = 0; py < LIVE_WIN_H; py++) {
				uint32_t *dst_row = fb + (y0 + 16U + py) * SIMSO_FB_W + (x0 + 2U);
				const uint32_t *src_row = s_win_tmp + py * LIVE_WIN_W;
				for (uint32_t px = 0; px < LIVE_WIN_W; px++)
					dst_row[px] = src_row[px];
			}
		} else {
			zgfx_text(&cv, x0 + 28U, y0 + 46U, "AGUARDANDO...", COL_MISS, 1);
		}
	}
}

/* Key hints, drawn in the free strip that render_gantt() leaves under the bottom panel.
 * Keep this list in sync with the key handling in simso_main(). */
static void draw_key_hints(void)
{
	static const struct { const char *key; const char *what; } hints[] = {
		{ "ESPACO", "passo" },
		{ "ENTER",  "completo" },
		{ "1-7",    "algoritmo" },
		{ "ESC",    "sair" },
	};
	struct zcanvas cv = { s_fb, SIMSO_FB_W, SIMSO_FB_H };
	uint32_t x = 24U, y = SIMSO_FB_H - 14U;

	for (uint32_t i = 0; i < sizeof(hints) / sizeof(hints[0]); i++) {
		uint32_t klen = 0, wlen = 0;
		while (hints[i].key[klen]) klen++;
		while (hints[i].what[wlen]) wlen++;
		zgfx_text(&cv, x, y, hints[i].key, COL_TEXT, 1);
		x += (klen + 1U) * 8U; /* key + one space; the font is 8 px wide */
		zgfx_text(&cv, x, y, hints[i].what, COL_MISS, 1);
		x += wlen * 8U + 24U;  /* label + gap before the next hint */
	}
}

static void draw_frame(const struct sim *s, uint32_t vis)
{
	render_gantt(s_fb, SIMSO_FB_W, SIMSO_FB_H, s, vis);
	composite_live_windows(s_fb, s, vis);
	draw_key_hints();
	sys_fb_blit(s_fb, SIMSO_FB_W, SIMSO_FB_H);
}

#define RUN_KEY 1 /* run_kernel_sim() stopped waiting because a hotkey was pressed */

/* Scancodes that must react at once, even while a simulation is still running. */
static bool is_hotkey(int32_t sc)
{
	return sc == 0x01 || sc == 0x10          /* Esc / Q */
	    || (sc >= 0x02 && sc <= 0x08)        /* 1-7: algorithm */
	    || sc == 0x39 || sc == 0x1C;         /* Space / Enter */
}

/* Starts the simulation in the kernel and draws it live. Returns 0 when it finished,
 * -1 on error, or RUN_KEY (with *key set) if a hotkey was pressed before it finished. */
static int run_kernel_sim(uint32_t alg, int32_t *key)
{
	copy_sim(&s_sim, &s_base_cfg);
	s_sim.alg = alg;
	if (sys_sim_start(&s_sim) < 0) return -1;

	uint32_t last_draw_ms = 0;
	int64_t r;
	while ((r = sys_sim_poll(&s_sim)) == 0) {
		int32_t sc = sys_poll_scancode();
		if (sc > 0 && !(sc & 0x80) && is_hotkey(sc)) {
			*key = sc;
			return RUN_KEY;
		}

		uint32_t now_ms = sys_ticks_ms();
		if (now_ms - last_draw_ms >= 40U) {
			draw_frame(&s_sim, s_sim.now);
			last_draw_ms = now_ms;
		} else {
			asm volatile("pause");
		}
	}
	return (r > 0) ? 0 : -1;
}

/* What the screen should show once a simulation is ready. */
enum view { VIEW_DEFAULT = 0, VIEW_STEP, VIEW_FULL };

/* Applies the view requested by a key pressed during the run. */
static void apply_view(enum view v, uint32_t *vis, uint32_t *delay_ms)
{
	if (v == VIEW_STEP) {
		*delay_ms = 0;
		*vis = 1;
	} else if (v == VIEW_FULL) {
		*delay_ms = 0;
		*vis = s_sim.total_ticks;
	}
}

/* Runs the simulation and reacts to keys pressed while it runs:
 *   1-7        restart at once with that algorithm;
 *   Space      cut the animation short and show the result in step-by-step mode;
 *   Enter      cut the animation short and show the full result;
 *   Esc / Q    quit.
 * The kernel runs only the first simulation of a session at the slow, live pace; every
 * restart is fast, so these keys answer immediately.
 * Returns 0 when a result is ready, 1 to quit, -1 on error. *alg may change. */
static int run_interactive(uint32_t *alg, enum view *view)
{
	*view = VIEW_DEFAULT;
	for (;;) {
		int32_t key = 0;
		int r = run_kernel_sim(*alg, &key);
		if (r != RUN_KEY)
			return r;
		if (key == 0x01 || key == 0x10) {
			return 1;
		} else if (key >= 0x02 && key <= 0x08) {
			*alg = (uint32_t)(key - 0x02);
			*view = VIEW_DEFAULT;
		} else if (key == 0x39) {
			*view = VIEW_STEP;
		} else if (key == 0x1C) {
			*view = VIEW_FULL;
		}
	}
}

int simso_main(void)
{
	/* Drop key events queued before this program started: the command line typed in the
	 * shell (its spaces, digits and Enter) would otherwise be read as the Space / 1-7 /
	 * Enter hotkeys below and change the algorithm or the step mode on its own. */
	while (sys_poll_scancode() > 0) {}

	uint32_t alg = ALG_RR;
	uint32_t delay_ms = 0;
	int has_seed = 0;
	uint32_t seed_override = 0;
	int has_quantum = 0;
	uint32_t quantum_override = 2;
	int has_overhead = 0;
	uint32_t overhead_override = 1;
	const char *input_path = NULL;

	/* Parse CLI arguments passed from Zanux shell (`simso --alg CFS --input CASO1.JSON --delay 150`). */
	char args_buf[256];
	char *argv[32];
	int argc = 0;
	int64_t alen = sys_get_args(args_buf, sizeof(args_buf));
	if (alen > 0) {
		char *p = args_buf;
		while (*p && argc < 32) {
			while (*p == ' ' || *p == '\t') p++;
			if (!*p) break;
			argv[argc++] = p;
			while (*p && *p != ' ' && *p != '\t') p++;
			if (*p) *p++ = '\0';
		}
	}

	for (int i = 0; i < argc; i++) {
		if ((streq_ci(argv[i], "--alg") || streq_ci(argv[i], "-a")) && i + 1 < argc) {
			int a = parse_alg(argv[++i]);
			if (a >= 0) alg = (uint32_t)a;
		} else if ((streq_ci(argv[i], "--input") || streq_ci(argv[i], "-i")) && i + 1 < argc) {
			input_path = argv[++i];
		} else if (streq_ci(argv[i], "--seed") && i + 1 < argc) {
			seed_override = parse_u32(argv[++i], 42);
			has_seed = 1;
		} else if (streq_ci(argv[i], "--delay") && i + 1 < argc) {
			delay_ms = parse_u32(argv[++i], 0);
		} else if (streq_ci(argv[i], "--quantum") && i + 1 < argc) {
			quantum_override = parse_u32(argv[++i], 2);
			has_quantum = 1;
		} else if ((streq_ci(argv[i], "--overhead") || streq_ci(argv[i], "--sobrecarga")) && i + 1 < argc) {
			overhead_override = parse_u32(argv[++i], 1);
			has_overhead = 1;
		}
	}

	/* Load test case: --input file -> default CASO1.JSON on FAT32 -> built-in Caso Base. */
	s_base_cfg.alg = alg;
	bool loaded = false;
	if (input_path)
		loaded = load_json_file(input_path, &s_base_cfg);
	if (!loaded)
		loaded = load_json_file("CASO1.JSON", &s_base_cfg);
	if (!loaded)
		load_caso_base(&s_base_cfg, alg);

	if (has_seed) s_base_cfg.seed = seed_override;
	if (has_quantum) s_base_cfg.quantum = quantum_override;
	if (has_overhead) s_base_cfg.overhead = overhead_override;

	enum view view;
	int rr = run_interactive(&alg, &view);
	if (rr < 0) return 1;
	if (rr > 0) return 0; /* Esc / Q pressed during the first run */

	uint32_t vis = (delay_ms > 0 && s_sim.total_ticks > 0) ? 1 : s_sim.total_ticks;
	apply_view(view, &vis, &delay_ms);
	uint32_t last_step_ms = sys_ticks_ms();
	draw_frame(&s_sim, vis);

	for (;;) {
		int dirty = 0;

		if (delay_ms > 0 && vis < s_sim.total_ticks) {
			uint32_t now_ms = sys_ticks_ms();
			if (now_ms - last_step_ms >= delay_ms) {
				vis++;
				last_step_ms = now_ms;
				dirty = 1;
			}
		}

		int32_t sc = sys_poll_scancode();
		if (sc > 0 && !(sc & 0x80)) {
			if (sc == 0x01 || sc == 0x10) return 0; /* ESC / Q */
			else if (sc >= 0x02 && sc <= 0x08) {
				alg = (uint32_t)(sc - 0x02);
				enum view v;
				int rr2 = run_interactive(&alg, &v);
				if (rr2 > 0) return 0; /* Esc / Q pressed while it ran */
				if (rr2 == 0) {
					vis = (delay_ms > 0 && s_sim.total_ticks > 0) ? 1 : s_sim.total_ticks;
					apply_view(v, &vis, &delay_ms);
					last_step_ms = sys_ticks_ms();
					dirty = 1;
				}
			} else if (sc == 0x39) { /* Space: step-by-step */
				delay_ms = 0;
				vis = (vis >= s_sim.total_ticks) ? 1 : (vis + 1);
				dirty = 1;
			} else if (sc == 0x1C) { /* Enter: full Gantt */
				delay_ms = 0;
				vis = s_sim.total_ticks;
				dirty = 1;
			}
		} else if (!dirty) {
			asm volatile("pause");
		}

		if (dirty)
			draw_frame(&s_sim, vis);
	}
}
