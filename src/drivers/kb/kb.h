/**
 * @file kb.h
 * @brief PS/2 keyboard driver (IRQ1, Set-1 scancodes, 256-byte ring buffers).
 *
 * Maintains two separate 256-byte circular buffers (255 usable entries each):
 * one for decoded ASCII characters (with Shift and CapsLock state) and one for
 * raw Set-1 scancodes. When a buffer is full, newly arriving bytes are dropped.
 */

#ifndef DRIVERS_KB_KB_H
#define DRIVERS_KB_KB_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Pop the next decoded ASCII character from the keyboard ring buffer.
 *
 * @return Decoded character in range 0..255, or -1 if the buffer is empty.
 */
int32_t kb_getchar(void);

/**
 * @brief Check whether at least one decoded ASCII character is available.
 *
 * @return true if kb_getchar() would return a character; false if empty.
 */
bool kb_has_char(void);

/**
 * @brief Pop the next raw PS/2 Set-1 scancode from the raw scancode ring buffer.
 *
 * Includes both make (press) and break (release, bit 7 set) scancodes.
 *
 * @return Raw scancode in range 0..255, or -1 if the raw buffer is empty.
 */
int32_t kb_pop_scancode(void);

/**
 * @brief Reset both software ring buffers and drain the 8042 controller FIFO.
 *
 * Clears the ASCII and raw scancode buffers, then reads port 0x60 while bit 0
 * (output buffer full) of status port 0x64 is set.
 */
void kb_drain(void);

/**
 * @brief IRQ1 keyboard interrupt handler.
 *
 * Reads one scancode from port 0x60, pushes it to the raw scancode buffer,
 * updates Left/Right Shift (0x2A/0x36, release 0xAA/0xB6) and CapsLock (0x3A)
 * state, and pushes any resulting printable/control ASCII character to the
 * character ring buffer.
 */
void kb_handler(void);

#endif // DRIVERS_KB_KB_H
