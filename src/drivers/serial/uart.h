/**
 * @file uart.h
 * @brief Polling 16550/8250 UART serial output driver (COM1 at 0x3F8).
 */

#ifndef DRIVERS_SERIAL_UART_H
#define DRIVERS_SERIAL_UART_H

#include <stdint.h>

/**
 * @brief Transmit a single byte over COM1 (0x3F8), blocking until THR is empty.
 *
 * Polls Line Status Register (0x3FD) bit 5 (THRE). Converts `\n` into `\r\n`.
 *
 * @param c Byte to transmit.
 */
void serial_putc(uint8_t c);

/**
 * @brief Transmit a NUL-terminated string over COM1.
 *
 * @param s Pointer to a NUL-terminated string (no-op if NULL).
 */
void serial_puts(const char *s);

#endif // DRIVERS_SERIAL_UART_H
