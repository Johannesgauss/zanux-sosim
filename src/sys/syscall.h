/**
 * @file syscall.h
 * @brief Kernel syscall numbers and dispatch table declarations.
 */

#ifndef SYS_SYSCALL_H
#define SYS_SYSCALL_H

#include "util/alias.h"
#include <stdint.h>

#define SYS_EXIT          0
#define SYS_WRITE         1
#define SYS_READ          2
#define SYS_EXEC          3
#define SYS_ALLOC         4
#define SYS_READ_FILE     5
#define SYS_FB_BLIT       6
#define SYS_POLL_SCANCODE 7
#define SYS_TICKS_MS      8
#define SYS_PCSPK         9
#define SYS_OPL_IO        10
#define SYS_READDIR       11

/** Total number of registered system calls in `syscall_table`. */
#define SYSCALL_NUMBER 12

/** Generic 6-argument syscall handler function pointer type. */
typedef int64_t (*sys_call_t)(umax, umax, umax, umax, umax, umax);

/** Global syscall dispatch table indexed by `%rax` in `syscall_handler`. */
extern const sys_call_t syscall_table[SYSCALL_NUMBER];

#endif // SYS_SYSCALL_H
