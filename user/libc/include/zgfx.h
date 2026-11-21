/**
 * @file zgfx.h
 * @brief Lightweight 2D graphics & UI widget library for Zanux Ring 3 apps (and host PPM previews).
 *
 * All functions operate on a caller-owned 32-bit RGB pixel buffer (`0x00RRGGBB`)
 * wrapped in a `struct zcanvas` and perform bounds clipping automatically so
 * out-of-range coordinates never corrupt memory.
 *
 * Quick example:
 * @code
 *   static uint32_t fb[640 * 400];
 *   struct zcanvas c = { .fb = fb, .w = 640, .h = 400 };
 *   zgfx_clear(&c, 0x0012161C);
 *   zgfx_window(&c, 16, 16, 200, 120, "P1 [Plasma]", "OK", 0x002ECC71,
 *               0x001B222C, 0x002B3544, 0x003B4554);
 *   zgfx_progress(&c, 24, 116, 184, 8, 75, 0x0000BCD4, 0x0012161C);
 *   zgfx_printf(&c, 24, 48, 0x00FFFFFF, 1, "Ticks: %u  Avg: %.2f", 16, 4.33);
 *   sys_fb_blit(c.fb, c.w, c.h);
 * @endcode
 */

#ifndef ZANUX_LIBC_ZGFX_H
#define ZANUX_LIBC_ZGFX_H

#include <stdint.h>

/**
 * @brief Target 2D pixel buffer descriptor passed to all `zgfx_*` functions.
 */
struct zcanvas {
	uint32_t *fb; /**< Pointer to `w * h` pixels in `0x00RRGGBB` format. */
	uint32_t w;   /**< Canvas width in pixels. */
	uint32_t h;   /**< Canvas height in pixels. */
};

/**
 * @brief Pack 8-bit red, green, and blue channels into a `0x00RRGGBB` pixel color.
 *
 * @param r Red intensity (`0..255`).
 * @param g Green intensity (`0..255`).
 * @param b Blue intensity (`0..255`).
 * @return Packed 32-bit RGB color suitable for `zgfx_*` calls.
 */
static inline uint32_t zgfx_rgb(uint8_t r, uint8_t g, uint8_t b)
{
	return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/**
 * @brief Fill the entire canvas with a single solid color.
 *
 * @param c   Target canvas.
 * @param col Fill color (`0x00RRGGBB`).
 */
void zgfx_clear(struct zcanvas *c, uint32_t col);

/**
 * @brief Draw a single pixel at `(x, y)` (silently ignored if out of bounds).
 *
 * @param c   Target canvas.
 * @param x   Horizontal coordinate (`0..w-1`).
 * @param y   Vertical coordinate (`0..h-1`).
 * @param col Pixel color (`0x00RRGGBB`).
 */
void zgfx_pixel(struct zcanvas *c, uint32_t x, uint32_t y, uint32_t col);

/**
 * @brief Draw a 1-pixel-thick horizontal line starting at `(x, y)` going right.
 *
 * @param c   Target canvas.
 * @param x   Starting X coordinate.
 * @param y   Y coordinate of the line.
 * @param len Line length in pixels (clipped to canvas width).
 * @param col Line color (`0x00RRGGBB`).
 */
void zgfx_hline(struct zcanvas *c, uint32_t x, uint32_t y, uint32_t len, uint32_t col);

/**
 * @brief Draw a 1-pixel-thick vertical line starting at `(x, y)` going down.
 *
 * Useful for Gantt tick grids and vertical absolute deadline markers.
 *
 * @param c   Target canvas.
 * @param x   X coordinate of the line.
 * @param y   Starting Y coordinate.
 * @param len Line length in pixels (clipped to canvas height).
 * @param col Line color (`0x00RRGGBB`).
 */
void zgfx_vline(struct zcanvas *c, uint32_t x, uint32_t y, uint32_t len, uint32_t col);

/**
 * @brief Draw a filled rectangle (clipped to canvas bounds).
 *
 * Useful for Gantt bars, Game of Life cells, and Bubble Sort bars.
 *
 * @param c   Target canvas.
 * @param x   Top-left X coordinate.
 * @param y   Top-left Y coordinate.
 * @param w   Rectangle width in pixels.
 * @param h   Rectangle height in pixels.
 * @param col Fill color (`0x00RRGGBB`).
 */
void zgfx_rect(struct zcanvas *c, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t col);

/**
 * @brief Draw a 1-pixel-thick hollow rectangle border (clipped to canvas bounds).
 *
 * @param c   Target canvas.
 * @param x   Top-left X coordinate.
 * @param y   Top-left Y coordinate.
 * @param w   Outer width in pixels.
 * @param h   Outer height in pixels.
 * @param col Border color (`0x00RRGGBB`).
 */
void zgfx_rect_outline(struct zcanvas *c, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t col);

/**
 * @brief Draw a filled UI panel/card with a 1-pixel border.
 *
 * @param c      Target canvas.
 * @param x      Top-left X coordinate.
 * @param y      Top-left Y coordinate.
 * @param w      Panel width in pixels.
 * @param h      Panel height in pixels.
 * @param bg     Interior fill color (`0x00RRGGBB`).
 * @param border 1-pixel outline color (`0x00RRGGBB`).
 */
void zgfx_panel(struct zcanvas *c, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                uint32_t bg, uint32_t border);

/**
 * @brief Draw a titled UI window/pane with an optional right-aligned status badge.
 *
 * Draws a bordered panel with an 18-pixel title bar at the top, left-aligned
 * @p title text (white), and right-aligned @p status text (colored with @p status_col)
 * — e.g. `P1 [Plasma+Primos]` on the left and `OK` / `ESTOUR` on the right.
 *
 * @param c          Target canvas.
 * @param x          Top-left X coordinate.
 * @param y          Top-left Y coordinate.
 * @param w          Window width in pixels.
 * @param h          Window height in pixels.
 * @param title      Optional left-aligned title string (may be NULL).
 * @param status     Optional right-aligned status string (may be NULL).
 * @param status_col Color of @p status text (`0x00RRGGBB`).
 * @param bg         Body background color (`0x00RRGGBB`).
 * @param title_bg   Title-bar background color (`0x00RRGGBB`).
 * @param border     1-pixel outer border color (`0x00RRGGBB`).
 */
void zgfx_window(struct zcanvas *c, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                 const char *title, const char *status, uint32_t status_col,
                 uint32_t bg, uint32_t title_bg, uint32_t border);

/**
 * @brief Draw a horizontal progress bar filled to @p pct percent.
 *
 * @param c   Target canvas.
 * @param x   Top-left X coordinate.
 * @param y   Top-left Y coordinate.
 * @param w   Total bar width in pixels.
 * @param h   Bar height in pixels.
 * @param pct Completion percentage (`0..100`, clamped to 100).
 * @param fg  Filled portion color (`0x00RRGGBB`).
 * @param bg  Unfilled track color (`0x00RRGGBB`).
 */
void zgfx_progress(struct zcanvas *c, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                   uint32_t pct, uint32_t fg, uint32_t bg);

/**
 * @brief Render a single 8x8 ASCII bitmap character at `(x, y)` with integer scaling.
 *
 * @param c     Target canvas.
 * @param x     Top-left X coordinate.
 * @param y     Top-left Y coordinate.
 * @param ch    ASCII character (`0..127`; non-ASCII renders as `'?'`).
 * @param col   Glyph foreground color (`0x00RRGGBB`; background is transparent).
 * @param scale Integer pixel scale (`1` = 8x8 px, `2` = 16x16 px, etc.; 0 treated as 1).
 */
void zgfx_char(struct zcanvas *c, uint32_t x, uint32_t y, char ch, uint32_t col, uint32_t scale);

/**
 * @brief Render a NUL-terminated ASCII string at `(x, y)` with integer scaling.
 *
 * Each character advances X by `8 * scale` pixels.
 *
 * @param c     Target canvas.
 * @param x     Starting X coordinate.
 * @param y     Top Y coordinate.
 * @param s     NUL-terminated string (no-op if NULL).
 * @param col   Text color (`0x00RRGGBB`).
 * @param scale Integer pixel scale (`1` = normal 8x8, `2` = 16x16 header).
 */
void zgfx_text(struct zcanvas *c, uint32_t x, uint32_t y, const char *s, uint32_t col, uint32_t scale);

/**
 * @brief Format a `printf`-style string and render it onto the canvas at `(x, y)`.
 *
 * Supports `%s`, `%c`, `%d`, `%u`, `%x`/`%X`, `%p`, and `%.Nf` (up to 255 chars).
 *
 * @param c     Target canvas.
 * @param x     Starting X coordinate.
 * @param y     Top Y coordinate.
 * @param col   Text color (`0x00RRGGBB`).
 * @param scale Integer pixel scale (`1` = 8x8, `2` = 16x16).
 * @param fmt   `printf`-style format string followed by arguments.
 */
void zgfx_printf(struct zcanvas *c, uint32_t x, uint32_t y, uint32_t col, uint32_t scale,
                 const char *fmt, ...);

#endif // ZANUX_LIBC_ZGFX_H
