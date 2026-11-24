#include "sim.h"
extern char font8x8_basic[128][8]; /* Provided by user/libc/zgfx.c */

static const char *s_alg_names[ALG_COUNT] = {
	"FCFS", "SJF", "RR", "PRIO", "EDF", "CFS", "AUTORAL"
};

static void fill_rect(uint32_t *fb, uint32_t w, uint32_t h,
                      uint32_t x, uint32_t y, uint32_t rw, uint32_t rh, uint32_t col)
{
	if (x >= w || y >= h) return;
	uint32_t x_end = (x + rw > w) ? w : (x + rw);
	uint32_t y_end = (y + rh > h) ? h : (y + rh);
	for (uint32_t py = y; py < y_end; py++) {
		uint32_t *row = fb + py * w;
		for (uint32_t px = x; px < x_end; px++)
			row[px] = col;
	}
}

static void draw_vline(uint32_t *fb, uint32_t w, uint32_t h,
                       uint32_t x, uint32_t y0, uint32_t y1, uint32_t col)
{
	if (x >= w) return;
	if (y1 > h) y1 = h;
	for (uint32_t py = y0; py < y1; py++)
		fb[py * w + x] = col;
}

static void draw_char(uint32_t *fb, uint32_t w, uint32_t h,
                      uint32_t x, uint32_t y, char c, uint32_t col)
{
	uint8_t ch = ((uint8_t)c < 128) ? (uint8_t)c : '?';
	for (uint32_t row = 0; row < 8 && y + row < h; row++) {
		uint8_t bits = (uint8_t)font8x8_basic[ch][row];
		for (uint32_t bit = 0; bit < 8 && x + bit < w; bit++) {
			if (bits & (1U << bit))
				fb[(y + row) * w + (x + bit)] = col;
		}
	}
}

static void draw_text(uint32_t *fb, uint32_t w, uint32_t h,
                      uint32_t x, uint32_t y, const char *s, uint32_t col)
{
	if (!s) return;
	for (uint32_t i = 0; s[i]; i++)
		draw_char(fb, w, h, x + i * 8, y, s[i], col);
}

static void draw_uint(uint32_t *fb, uint32_t w, uint32_t h,
                      uint32_t x, uint32_t y, uint32_t val, uint32_t col)
{
	char buf[12], tmp[12];
	int pos = 0, n = 0;
	if (val == 0) {
		buf[pos++] = '0';
	} else {
		while (val > 0 && n < 11) { tmp[n++] = (char)('0' + (val % 10)); val /= 10; }
		while (n > 0) buf[pos++] = tmp[--n];
	}
	buf[pos] = '\0';
	draw_text(fb, w, h, x, y, buf, col);
}

/* Draws a fixed-point value scaled by 100 (e.g. 1234 -> "12.34"). */
static void draw_fixed2(uint32_t *fb, uint32_t w, uint32_t h,
						uint32_t x, uint32_t y, uint32_t v100, uint32_t col)
{
	char tmp[12], buf[16];
	uint32_t ip = v100 / 100, fp = v100 % 100;
	int n = 0, pos = 0;
	if (ip == 0)
		tmp[n++] = '0';
	while (ip > 0 && n < 11)
	{
		tmp[n++] = (char)('0' + ip % 10);
		ip /= 10;
	}
	while (n > 0)
		buf[pos++] = tmp[--n];
	buf[pos++] = '.';
	buf[pos++] = (char)('0' + fp / 10);
	buf[pos++] = (char)('0' + fp % 10);
	buf[pos] = '\0';
	draw_text(fb, w, h, x, y, buf, col);
}

/* Legend: a colored square + a label per item, in one row starting at (x, y). */
static void draw_legend(uint32_t *fb, uint32_t w, uint32_t h, uint32_t x, uint32_t y)
{
	static const struct
	{
		uint32_t col;
		const char *label;
	} items[] = {
		{COL_EXEC, "EXEC"},
		{COL_OVERHEAD, "OVERHEAD"},
		{COL_MISS, "ESTOURO"},
		{COL_DEADLINE, "DEADLINE"},
		{COL_IO, "IO"},
		{COL_IDLE, "OCIOSO"},
	};
	for (uint32_t i = 0; i < sizeof(items) / sizeof(items[0]); i++)
	{
		fill_rect(fb, w, h, x, y, 8, 8, items[i].col);
		draw_text(fb, w, h, x + 12, y, items[i].label, COL_TEXT);
		uint32_t len = 0;
		while (items[i].label[len])
			len++; /* no libc here: count the length by hand */
		x += 12 + len * 8 + 12;
	}
}

#define TBL_X0     24U /* left edge of the final table */
#define TBL_COL_W  40U /* column pitch: 5 characters of 8 px */
#define SUM_X0     24U /* left edge of the summary row */
#define SUM_PITCH  100U /* distance between summary items (label + value) */

/* Final table: header + one row per task, starting at row y.
 * Shows as many rows as fit on screen; the last line reports how many are hidden. */
static void draw_table(uint32_t *fb, uint32_t w, uint32_t h, const struct sim *s, uint32_t y)
{
	static const char *heads[10] = {
		"ID", "Cheg", "Exec", "Dead", "Prio", "Ini", "Term", "Esp", "TAT", "DeadOK"
	};
	/* Column x positions: equally spaced, derived from the table origin and column width. */
	uint32_t cx[10];
	for (uint32_t c = 0; c < 10; c++)
		cx[c] = TBL_X0 + c * TBL_COL_W;

	for (uint32_t c = 0; c < 10; c++)
		draw_text(fb, w, h, cx[c], y, heads[c], COL_TEXT);
	y += 12;

	/* How many 10 px rows fit before the bottom of the panel (h - 8). */
	uint32_t room = (y + 6 < h) ? (h - 6 - y) / 10 : 0;
	uint32_t shown = s->n;
	if (s->n > room)
		shown = (room > 0) ? room - 1 : 0; /* keep one line for the "hidden" note */

	for (uint32_t i = 0; i < shown; i++) {
		const struct task *t = &s->tasks[i];
		uint32_t ry = y + i * 10;
		draw_text(fb, w, h, cx[0], ry, t->name, COL_TEXT);
		draw_uint(fb, w, h, cx[1], ry, t->arrival,  COL_TEXT);
		draw_uint(fb, w, h, cx[2], ry, t->burst,    COL_TEXT);
		draw_uint(fb, w, h, cx[3], ry, t->deadline, COL_TEXT);
		draw_uint(fb, w, h, cx[4], ry, t->priority, COL_TEXT);
		if (t->state == TASK_DONE) {
			draw_uint(fb, w, h, cx[5], ry, t->start,      COL_TEXT);
			draw_uint(fb, w, h, cx[6], ry, t->finish,     COL_TEXT);
			draw_uint(fb, w, h, cx[7], ry, t->wait,       COL_TEXT);
			draw_uint(fb, w, h, cx[8], ry, t->turnaround, COL_TEXT);
			draw_text(fb, w, h, cx[9], ry, t->ok ? "SIM" : "NAO",
			          t->ok ? COL_EXEC : COL_OVERHEAD);
		} else {
			for (uint32_t c = 5; c < 10; c++)
				draw_text(fb, w, h, cx[c], ry, "-", COL_GRID);
		}
	}

	if (shown < s->n && room > 0) {
		draw_text(fb, w, h, cx[0], y + shown * 10, "MAIS:", COL_DEADLINE);
		draw_uint(fb, w, h, cx[0] + 6 * 8, y + shown * 10, s->n - shown, COL_DEADLINE);
	}
}

/* Overhead ticks are recorded with no owner (tl_task == -1). Attribute them to the
 * task that ran right before tick t. Returns -1 if no task ran before. */
static int prev_task(const struct sim *s, uint32_t t)
{
	while (t > 0) {
		t--;
		if (s->tl_task[t] >= 0)
			return s->tl_task[t];
	}
	return -1;
}

void render_gantt(uint32_t *fb, uint32_t w, uint32_t h, const struct sim *s, uint32_t vis)
{
	if (!fb || !s || w == 0 || h == 0) return;
	fill_rect(fb, w, h, 0, 0, w, h, COL_BG);

	/* Header bar with 7 algorithm slots + color legend. */
	fill_rect(fb, w, h, 16, 12, w - 32, 28, COL_PANEL);
	for (uint32_t a = 0; a < ALG_COUNT; a++)
		fill_rect(fb, w, h, 24 + a * 36, 18, 28, 16, (a == s->alg) ? COL_EXEC : COL_GRID);

	uint32_t total = (s->total_ticks > 0) ? s->total_ticks : s->now;
	if (total == 0) total = 1;
	uint32_t scale_ticks = (s->total_ticks > 0) ? s->total_ticks : ((total < 20U) ? 20U : total);
	if (vis > total) vis = total;

	/* Reserve y=46..136 for live Ring 3 process windows; the Gantt starts at y=142. */
	uint32_t gx = 48, gy = 142, gw = (w > 64) ? (w - 64) : w;

	/* Square cells: one cell (tick x task) is cs x cs pixels, so the tick width
	 * equals the row height. cs is the largest square that fits the width and
	 * the 152 px height budget, capped at 16 px so the table below keeps space. */
	uint32_t rows = (s->n > 0) ? s->n : 1; /* one row per task (no separate OV row) */
	uint32_t cs = gw / scale_ticks;
	if (152 / rows < cs) cs = 152 / rows;
	if (cs > 16) cs = 16;
	if (cs < 2) cs = 2;
	uint32_t row_h = cs, tick_w = cs;
	gw = cs * scale_ticks;  /* the panel is exactly as wide as the grid */
	uint32_t gh = cs * rows;
	fill_rect(fb, w, h, gx, gy, gw, gh, COL_PANEL);

	for (uint32_t r = 0; r <= rows; r++)
		if (gy + r * row_h < gy + gh)
			fill_rect(fb, w, h, gx, gy + r * row_h, gw, 1, COL_GRID);

	for (uint32_t t = 0; t <= scale_ticks; t++)
		if (gx + t * tick_w < gx + gw)
			draw_vline(fb, w, h, gx + t * tick_w, gy, gy + gh, COL_GRID);

	/* Timeline blocks up to `vis`. */
	for (uint32_t t = 0; t < vis && t < SIM_MAX_TICKS; t++) {
		uint8_t kind = s->tl_kind[t];
		int8_t tidx = s->tl_task[t];
		int row = -1;
		uint32_t col = COL_EXEC;

		if (kind == TICK_EXEC || kind == TICK_MISS) {
			row = tidx;
			col = (kind == TICK_MISS) ? COL_MISS : COL_EXEC;
		} else if (kind == TICK_OVERHEAD) {
			row = prev_task(s, t); /* overhead belongs to the task that ran before */
			col = COL_OVERHEAD;
		} else if (kind == TICK_IO) {
			row = (tidx >= 0) ? tidx : prev_task(s, t);
			col = COL_IO;
		} /* TICK_IDLE: nothing to draw, the panel background shows through */

		if (row >= 0 && (uint32_t)row < s->n) {
			/* Square block inset by 1 px inside its cell. */
			uint32_t side = (cs > 2) ? (cs - 2) : 1;
			fill_rect(fb, w, h, gx + t * tick_w + 1, gy + (uint32_t)row * row_h + 1, side, side, col);
		}
	}

	/* Absolute deadline markers. */
	for (uint32_t i = 0; i < s->n; i++) {
		uint32_t x = gx + s->tasks[i].deadline * tick_w;
		if (x < gx + gw) {
			uint32_t y0 = gy + i * row_h + 1;
			draw_vline(fb, w, h, x, y0, y0 + row_h - 2, COL_DEADLINE);
			if (x + 1 < gx + gw)
				draw_vline(fb, w, h, x + 1, y0, y0 + row_h - 2, COL_DEADLINE);
		}
	}

	if (vis < total && gx + vis * tick_w < gx + gw)
		draw_vline(fb, w, h, gx + vis * tick_w, gy, gy + gh, COL_TEXT);

	/* Row labels (`P1`, `P2`, ...). */
	for (uint32_t i = 0; i < s->n; i++)
		draw_text(fb, w, h, 16, gy + i * row_h + (row_h > 8 ? (row_h - 8) / 2 : 0), s->tasks[i].name, COL_TEXT);

	/* Bottom panel header (UI teammate expands table + averages here). */
	draw_legend(fb, w, h, 48, gy + gh + 4);
	uint32_t by = gy + gh + 20;
	fill_rect(fb, w, h, 16, by, w - 32, h - (by + 8), COL_PANEL);
	draw_text(fb, w, h, 24,  by + 8, "ALG:", COL_TEXT);
	draw_text(fb, w, h, 64,  by + 8, (s->alg < ALG_COUNT) ? s_alg_names[s->alg] : "?", COL_EXEC);
	draw_text(fb, w, h, 140, by + 8, "TICKS:", COL_TEXT);
	draw_uint(fb, w, h, 196, by + 8, (s->total_ticks > 0) ? s->total_ticks : s->now, COL_TEXT);
	draw_text(fb, w, h, 244, by + 8, "CTXSW:", COL_TEXT);
	draw_uint(fb, w, h, 300, by + 8, s->ctx_switches, COL_OVERHEAD);
	draw_text(fb, w, h, 348, by + 8, "Q:", COL_TEXT);
	draw_uint(fb, w, h, 368, by + 8, s->quantum, COL_TEXT);
	draw_text(fb, w, h, 404, by + 8, "OV:", COL_TEXT);
	draw_uint(fb, w, h, 432, by + 8, s->overhead, COL_TEXT);

	/* Quantitative summary (finished tasks only), fixed-point scaled by 100. */
	uint32_t done = 0, sum_wait = 0, sum_tat = 0;
	for (uint32_t i = 0; i < s->n; i++)
	{
		if (s->tasks[i].state != TASK_DONE)
			continue;
		done++;
		sum_wait += s->tasks[i].wait;
		sum_tat += s->tasks[i].turnaround;
	}
	uint32_t tt = (s->total_ticks > 0) ? s->total_ticks : s->now;
	uint32_t avg_wait = done ? (sum_wait * 100) / done : 0;
	uint32_t avg_tat = done ? (sum_tat * 100) / done : 0;
	uint32_t thr = tt ? (done * 100) / tt : 0; /* tasks per tick */
	uint32_t idle_pct = tt ? (s->idle_ticks * 100) / tt : 0;

	uint32_t ry = by + 22;
	/* Each item: label at SUM_X0 + k * SUM_PITCH, value right after it
	 * (label length + 1 space, 8 px per character). */
	uint32_t sx = SUM_X0;
	draw_text(fb, w, h, sx, ry, "ESP:", COL_TEXT);
	draw_fixed2(fb, w, h, sx + 5 * 8, ry, avg_wait, COL_EXEC);
	sx += SUM_PITCH;
	draw_text(fb, w, h, sx, ry, "TAT:", COL_TEXT);
	draw_fixed2(fb, w, h, sx + 5 * 8, ry, avg_tat, COL_EXEC);
	sx += SUM_PITCH;
	draw_text(fb, w, h, sx, ry, "THR:", COL_TEXT);
	draw_fixed2(fb, w, h, sx + 5 * 8, ry, thr, COL_EXEC);
	sx += SUM_PITCH;
	draw_text(fb, w, h, sx, ry, "OCIO:", COL_TEXT);
	draw_uint(fb, w, h, sx + 6 * 8, ry, idle_pct, COL_EXEC);
	draw_text(fb, w, h, sx + 6 * 8 + 24, ry, "%", COL_EXEC);

	draw_table(fb, w, h, s, ry + 14);
}
