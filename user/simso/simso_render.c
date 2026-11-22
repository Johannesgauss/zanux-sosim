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

void render_gantt(uint32_t *fb, uint32_t w, uint32_t h, const struct sim *s, uint32_t vis)
{
	if (!fb || !s || w == 0 || h == 0) return;
	fill_rect(fb, w, h, 0, 0, w, h, COL_BG);

	/* Header bar with 7 algorithm slots + color legend. */
	fill_rect(fb, w, h, 16, 12, w - 32, 28, COL_PANEL);
	for (uint32_t a = 0; a < ALG_COUNT; a++)
		fill_rect(fb, w, h, 24 + a * 36, 18, 28, 16, (a == s->alg) ? COL_EXEC : COL_GRID);
	fill_rect(fb, w, h, w - 160, 20, 14, 12, COL_EXEC);
	fill_rect(fb, w, h, w - 132, 20, 14, 12, COL_OVERHEAD);
	fill_rect(fb, w, h, w - 104, 20, 14, 12, COL_MISS);
	fill_rect(fb, w, h, w - 76,  20, 14, 12, COL_DEADLINE);

	uint32_t total = (s->total_ticks > 0) ? s->total_ticks : s->now;
	if (total == 0) total = 1;
	uint32_t scale_ticks = (s->total_ticks > 0) ? s->total_ticks : ((total < 20U) ? 20U : total);
	if (vis > total) vis = total;

	/* Reserve y=46..136 for live Ring 3 process windows; place Gantt at y=142..294. */
	uint32_t gx = 48, gy = 142, gw = (w > 64) ? (w - 64) : w, gh = 152;
	fill_rect(fb, w, h, gx, gy, gw, gh, COL_PANEL);

	uint32_t rows = s->n + 1;
	uint32_t row_h = (rows > 0 && gh / rows >= 12) ? (gh / rows) : 12;
	for (uint32_t r = 0; r <= rows; r++)
		if (gy + r * row_h < gy + gh)
			fill_rect(fb, w, h, gx, gy + r * row_h, gw, 1, COL_GRID);

	uint32_t tick_w = (gw / scale_ticks >= 2) ? (gw / scale_ticks) : 2;
	for (uint32_t t = 0; t <= scale_ticks; t++)
		if (gx + t * tick_w < gx + gw)
			draw_vline(fb, w, h, gx + t * tick_w, gy, gy + gh, COL_GRID);

	/* Timeline blocks up to `vis`. */
	for (uint32_t t = 0; t < vis && t < SIM_MAX_TICKS; t++) {
		uint8_t kind = s->tl_kind[t];
		int8_t tidx = s->tl_task[t];
		uint32_t x = gx + t * tick_w + 1;
		uint32_t bw = (tick_w > 2) ? (tick_w - 2) : 1;
		uint32_t bh = (row_h > 8) ? (row_h - 8) : 4;

		if ((kind == TICK_EXEC || kind == TICK_MISS) && tidx >= 0 && (uint32_t)tidx < s->n) {
			uint32_t y = gy + (uint32_t)tidx * row_h + 4;
			fill_rect(fb, w, h, x, y, bw, bh, (kind == TICK_MISS) ? COL_MISS : COL_EXEC);
		} else if (kind == TICK_OVERHEAD || kind == TICK_IDLE || kind == TICK_IO) {
			uint32_t y = gy + s->n * row_h + 4;
			uint32_t col = (kind == TICK_OVERHEAD) ? COL_OVERHEAD : (kind == TICK_IO ? COL_IO : COL_IDLE);
			fill_rect(fb, w, h, x, y, bw, bh, col);
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

	/* Row labels (`P1`, `P2`, ..., `OV`). */
	for (uint32_t i = 0; i < s->n; i++)
		draw_text(fb, w, h, 16, gy + i * row_h + (row_h > 8 ? (row_h - 8) / 2 : 0), s->tasks[i].name, COL_TEXT);
	draw_text(fb, w, h, 16, gy + s->n * row_h + (row_h > 8 ? (row_h - 8) / 2 : 0), "OV", COL_OVERHEAD);

	/* Bottom panel header (UI teammate expands table + averages here). */
	uint32_t by = gy + gh + 10;
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
}
