#include <zgfx.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "drivers/gfx/font8x8.h"

void zgfx_clear(struct zcanvas *c, uint32_t col)
{
	if (!c || !c->fb) return;
	uint32_t total = c->w * c->h;
	for (uint32_t i = 0; i < total; i++)
		c->fb[i] = col;
}

void zgfx_pixel(struct zcanvas *c, uint32_t x, uint32_t y, uint32_t col)
{
	if (!c || !c->fb || x >= c->w || y >= c->h) return;
	c->fb[y * c->w + x] = col;
}

void zgfx_hline(struct zcanvas *c, uint32_t x, uint32_t y, uint32_t len, uint32_t col)
{
	if (!c || !c->fb || y >= c->h || x >= c->w) return;
	uint32_t end = (x + len > c->w) ? c->w : (x + len);
	uint32_t *row = c->fb + y * c->w;
	for (uint32_t px = x; px < end; px++)
		row[px] = col;
}

void zgfx_vline(struct zcanvas *c, uint32_t x, uint32_t y, uint32_t len, uint32_t col)
{
	if (!c || !c->fb || x >= c->w || y >= c->h) return;
	uint32_t end = (y + len > c->h) ? c->h : (y + len);
	for (uint32_t py = y; py < end; py++)
		c->fb[py * c->w + x] = col;
}

void zgfx_rect(struct zcanvas *c, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t col)
{
	if (!c || !c->fb || x >= c->w || y >= c->h) return;
	uint32_t xe = (x + w > c->w) ? c->w : (x + w);
	uint32_t ye = (y + h > c->h) ? c->h : (y + h);
	for (uint32_t py = y; py < ye; py++) {
		uint32_t *row = c->fb + py * c->w;
		for (uint32_t px = x; px < xe; px++)
			row[px] = col;
	}
}

void zgfx_rect_outline(struct zcanvas *c, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t col)
{
	if (w == 0 || h == 0) return;
	zgfx_hline(c, x, y, w, col);
	zgfx_hline(c, x, y + h - 1, w, col);
	zgfx_vline(c, x, y, h, col);
	zgfx_vline(c, x + w - 1, y, h, col);
}

void zgfx_panel(struct zcanvas *c, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                uint32_t bg, uint32_t border)
{
	zgfx_rect(c, x, y, w, h, bg);
	zgfx_rect_outline(c, x, y, w, h, border);
}

void zgfx_window(struct zcanvas *c, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                 const char *title, const char *status, uint32_t status_col,
                 uint32_t bg, uint32_t title_bg, uint32_t border)
{
	zgfx_panel(c, x, y, w, h, bg, border);
	uint32_t th = (h > 18) ? 18 : h;
	zgfx_rect(c, x + 1, y + 1, (w > 2) ? w - 2 : 0, (th > 1) ? th - 1 : 0, title_bg);
	if (title)
		zgfx_text(c, x + 6, y + 5, title, 0x00FFFFFFU, 1);
	if (status) {
		uint32_t slen = (uint32_t)strlen(status) * 8U;
		if (w > slen + 8)
			zgfx_text(c, x + w - slen - 6, y + 5, status, status_col, 1);
	}
}

void zgfx_progress(struct zcanvas *c, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                   uint32_t pct, uint32_t fg, uint32_t bg)
{
	if (pct > 100) pct = 100;
	zgfx_rect(c, x, y, w, h, bg);
	uint32_t fw = (w * pct) / 100U;
	if (fw > 0)
		zgfx_rect(c, x, y, fw, h, fg);
}

void zgfx_char(struct zcanvas *c, uint32_t x, uint32_t y, char ch, uint32_t col, uint32_t scale)
{
	if (!c || !c->fb) return;
	if (scale == 0) scale = 1;
	uint8_t idx = ((uint8_t)ch < 128) ? (uint8_t)ch : '?';
	for (uint32_t row = 0; row < 8; row++) {
		uint8_t bits = (uint8_t)font8x8_basic[idx][row];
		for (uint32_t bit = 0; bit < 8; bit++) {
			if (bits & (1U << bit)) {
				if (scale == 1)
					zgfx_pixel(c, x + bit, y + row, col);
				else
					zgfx_rect(c, x + bit * scale, y + row * scale, scale, scale, col);
			}
		}
	}
}

void zgfx_text(struct zcanvas *c, uint32_t x, uint32_t y, const char *s, uint32_t col, uint32_t scale)
{
	if (!s) return;
	if (scale == 0) scale = 1;
	for (uint32_t i = 0; s[i]; i++)
		zgfx_char(c, x + i * 8U * scale, y, s[i], col, scale);
}

void zgfx_printf(struct zcanvas *c, uint32_t x, uint32_t y, uint32_t col, uint32_t scale,
                 const char *fmt, ...)
{
	char buf[256];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	zgfx_text(c, x, y, buf, col, scale);
}
