/**
 * @file console.h
 * @brief Limine 32-bit framebuffer text console with COM1 serial mirroring.
 *
 * Renders 8x8 glyphs from `font8x8_basic` onto the primary Limine framebuffer
 * using the global `cursor` position, and mirrors every character to COM1 via
 * serial_putc().
 */

#ifndef DRIVERS_GFX_CONSOLE_H
#define DRIVERS_GFX_CONSOLE_H

#include <stdint.h>

/**
 * @brief Clear the Limine framebuffer to black (0) and reset `cursor` to (0, 0).
 *
 * Safe to call even if no framebuffer is present (still resets `cursor`).
 */
void gfx_clear_screen(void);

/**
 * @brief Scroll the framebuffer up by one 8-pixel text row if needed.
 *
 * Triggers when `cursor.y >= (fb.height / 8) - 1`, copying rows 1..N-1 up by
 * 8 scanlines, zeroing the bottom row, and clamping `cursor.y` to `max_rows - 2`.
 */
void gfx_scroll_if_needed(void);

/**
 * @brief Print a single ASCII character to COM1 serial and the framebuffer.
 *
 * Always mirrors @p c to serial_putc(). If no framebuffer is available, falls
 * back to print_char(). Handles `\n` (newline + scroll) and wraps to the next
 * line at `max_cols - 1`. Glyphs are indexed with `c & 0x7F`.
 *
 * @param c ASCII character to print.
 * @param r Red channel intensity (0..255).
 * @param g Green channel intensity (0..255).
 * @param b Blue channel intensity (0..255).
 */
void gfx_putc_color(uint8_t c, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Print a NUL-terminated string in RGB color.
 *
 * @param s Pointer to a NUL-terminated string (no-op if NULL).
 * @param r Red channel intensity (0..255).
 * @param g Green channel intensity (0..255).
 * @param b Blue channel intensity (0..255).
 */
void gfx_print_color(const char *s, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Print an unsigned 64-bit integer in base 10 in RGB color.
 *
 * @param val Integer value to format and print.
 * @param r   Red channel intensity (0..255).
 * @param g   Green channel intensity (0..255).
 * @param b   Blue channel intensity (0..255).
 */
void gfx_print_u64(uint64_t val, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Print an unsigned 64-bit integer with an optional suffix, right-padded with spaces.
 *
 * @param val    Integer value to format (up to 20 decimal digits).
 * @param w      Minimum field width in characters (pads with `' '` on the right).
 * @param suffix Optional NUL-terminated suffix appended after the digits (may be NULL).
 * @param r      Red channel intensity (0..255).
 * @param g      Green channel intensity (0..255).
 * @param b      Blue channel intensity (0..255).
 */
void gfx_print_u64_pad(uint64_t val, int32_t w, const char *suffix, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Print a fixed-point value scaled by 10 (e.g., 123 -> "12.3"), right-padded.
 *
 * @param val_x10 Value multiplied by 10 (prints `val_x10 / 10` followed by `.` and `val_x10 % 10`).
 * @param w       Minimum field width in characters (pads with `' '` on the right).
 * @param suffix  Optional NUL-terminated suffix appended after the decimal digit (may be NULL).
 * @param r       Red channel intensity (0..255).
 * @param g       Green channel intensity (0..255).
 * @param b       Blue channel intensity (0..255).
 */
void gfx_print_fixed10_pad(uint64_t val_x10, int32_t w, const char *suffix, uint8_t r, uint8_t g, uint8_t b);

#endif // DRIVERS_GFX_CONSOLE_H
