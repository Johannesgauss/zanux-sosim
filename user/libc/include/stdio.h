#ifndef ZANUX_LIBC_STDIO_H
#define ZANUX_LIBC_STDIO_H

#include <stdarg.h>
#include <stddef.h>

int putchar(int c);
int puts(const char *s);
int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
int snprintf(char *buf, size_t size, const char *fmt, ...);
int printf(const char *fmt, ...);

#endif // ZANUX_LIBC_STDIO_H
