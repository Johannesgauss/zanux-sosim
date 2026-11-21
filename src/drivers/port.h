/**
 * @file port.h
 * @brief x86_64 I/O port access primitives (`in`/`out` instructions).
 */

#ifndef DRIVERS_PORT_H
#define DRIVERS_PORT_H

#include <stdint.h>

/**
 * @brief Read an 8-bit byte from an x86 I/O port (`in al, dx`).
 *
 * @param port 16-bit I/O port address.
 * @return Byte read from @p port.
 */
uint8_t inb(uint16_t port);

/**
 * @brief Write an 8-bit byte to an x86 I/O port (`out dx, al`).
 *
 * @param port  16-bit I/O port address.
 * @param value Byte to write.
 * @return Echoes @p value.
 */
uint8_t outb(uint16_t port, uint8_t value);

/**
 * @brief Read a 16-bit word from an x86 I/O port (`in ax, dx`).
 *
 * @param port 16-bit I/O port address.
 * @return 16-bit word read from @p port.
 */
uint16_t inw(uint16_t port);

/**
 * @brief Write a 16-bit word to an x86 I/O port (`out dx, ax`).
 *
 * @param port  16-bit I/O port address.
 * @param value 16-bit word to write.
 */
void outw(uint16_t port, uint16_t value);

/**
 * @brief Perform a short I/O bus delay by writing 0 to diagnostic port 0x80.
 */
void io_wait(void);

#define outportb outb
#define inportb  inb

#endif // DRIVERS_PORT_H
