/**
 * @file io.h
 * @brief Standard stream I/O system calls (`sys_write`, `sys_read`).
 */

#ifndef SYS_IO_H
#define SYS_IO_H

#include <stddef.h>
#include <stdint.h>

/** Standard input file descriptor (PS/2 keyboard / COM1 serial). */
#define FD_STDIN  0
/** Standard output file descriptor (framebuffer console + COM1 mirror). */
#define FD_STDOUT 1
/** Standard error file descriptor (red framebuffer console + COM1 mirror). */
#define FD_STDERR 2

/**
 * @brief Write bytes to a standard output stream.
 *
 * Writes @p count bytes from @p buf to the framebuffer console and mirrors
 * them to COM1 serial. On `FD_STDOUT`, ASCII Form Feed (`'\x0C'`) clears the
 * screen; printable characters are rendered in phosphor green (`#33FF33`).
 * `FD_STDERR` renders characters in red.
 *
 * @param fd    Target file descriptor (`FD_STDOUT` or `FD_STDERR`).
 * @param buf   Source buffer containing bytes to write.
 * @param count Number of bytes to write.
 * @return Number of bytes written on success, or -1 on invalid @p fd or NULL @p buf.
 */
int64_t sys_write(int fd, const char *buf, size_t count);

/**
 * @brief Non-blocking read from standard input (`FD_STDIN`).
 *
 * Reads up to @p count available bytes from the PS/2 keyboard ring buffer
 * (falling back to COM1 serial if empty). Stops early on `'\n'`, `'\r'`, or
 * when no more input is immediately available.
 *
 * @param fd    Source file descriptor (must be `FD_STDIN`).
 * @param buf   Destination buffer in caller memory.
 * @param count Maximum number of bytes to read.
 * @return Number of bytes read (0 if no input is ready), or -1 on invalid arguments.
 */
int64_t sys_read(int fd, char *buf, size_t count);

#endif // SYS_IO_H
