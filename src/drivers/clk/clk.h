/**
 * @file clk.h
 * @brief 8253/8254 Programmable Interval Timer (PIT) channel 0 driver.
 */

#ifndef CLK_H
#define CLK_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Configure PIT channel 0 (ports 0x43/0x40) frequency in Hz.
 *
 * Programs mode 3 (square wave, lobyte/hibyte) with divisor `1193180 / safe_hz`.
 *
 * @param hz Desired interrupt frequency in Hz. Values <= 0 default to 100 Hz.
 */
void timer_phase(int32_t hz);

/**
 * @brief Initialize the PIT timer at 100 Hz.
 *
 * @return Always returns true.
 */
bool timer_add(void);

#endif // CLK_H
