#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "syscall.h"

static void emit(char *buf, size_t size, size_t *pos, char c)
{
	if (buf && size > 0 && *pos + 1 < size)
		buf[*pos] = c;
	(*pos)++;
}

static void emit_str(char *buf, size_t size, size_t *pos, const char *s)
{
	if (!s) s = "(null)";
	while (*s) emit(buf, size, pos, *s++);
}

static void emit_uint(char *buf, size_t size, size_t *pos, uint64_t val, unsigned base, int upper)
{
	const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
	char tmp[32];
	int n = 0;
	if (val == 0) {
		tmp[n++] = '0';
	} else {
		while (val > 0 && n < 31) {
			tmp[n++] = digits[val % base];
			val /= base;
		}
	}
	while (n > 0) emit(buf, size, pos, tmp[--n]);
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
	size_t pos = 0;
	for (const char *p = fmt; p && *p; p++) {
		if (*p != '%') {
			emit(buf, size, &pos, *p);
			continue;
		}
		p++;
		int prec = -1;
		if (*p == '.') {
			p++;
			prec = 0;
			while (*p >= '0' && *p <= '9') {
				prec = prec * 10 + (*p - '0');
				p++;
			}
		}
		int is_long = 0;
		while (*p == 'l' || *p == 'z') { is_long = 1; p++; }

		switch (*p) {
		case '%':
			emit(buf, size, &pos, '%');
			break;
		case 'c':
			emit(buf, size, &pos, (char)va_arg(ap, int));
			break;
		case 's':
			emit_str(buf, size, &pos, va_arg(ap, const char *));
			break;
		case 'd':
		case 'i': {
			int64_t v = is_long ? va_arg(ap, long) : va_arg(ap, int);
			if (v < 0) {
				emit(buf, size, &pos, '-');
				emit_uint(buf, size, &pos, (uint64_t)(-v), 10, 0);
			} else {
				emit_uint(buf, size, &pos, (uint64_t)v, 10, 0);
			}
			break;
		}
		case 'u': {
			uint64_t v = is_long ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
			emit_uint(buf, size, &pos, v, 10, 0);
			break;
		}
		case 'x':
		case 'X': {
			uint64_t v = is_long ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
			emit_uint(buf, size, &pos, v, 16, *p == 'X');
			break;
		}
		case 'p':
			emit_str(buf, size, &pos, "0x");
			emit_uint(buf, size, &pos, (uintptr_t)va_arg(ap, void *), 16, 0);
			break;
		case 'f': {
			double d = va_arg(ap, double);
			if (d < 0.0) { emit(buf, size, &pos, '-'); d = -d; }
			int dp = (prec >= 0) ? prec : 2;
			if (dp > 6) dp = 6;
			uint64_t ip = (uint64_t)d;
			double frac = d - (double)ip;
			emit_uint(buf, size, &pos, ip, 10, 0);
			if (dp > 0) {
				emit(buf, size, &pos, '.');
				for (int i = 0; i < dp; i++) {
					frac *= 10.0;
					int digit = (int)frac;
					if (digit < 0) digit = 0;
					if (digit > 9) digit = 9;
					emit(buf, size, &pos, (char)('0' + digit));
					frac -= (double)digit;
				}
			}
			break;
		}
		default:
			emit(buf, size, &pos, '%');
			if (*p) emit(buf, size, &pos, *p);
			break;
		}
	}
	if (buf && size > 0)
		buf[(pos < size) ? pos : (size - 1)] = '\0';
	return (int)pos;
}

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int r = vsnprintf(buf, size, fmt, ap);
	va_end(ap);
	return r;
}

int putchar(int c)
{
	char ch = (char)c;
	sys_write(1, &ch, 1);
	return c;
}

int puts(const char *s)
{
	if (!s) return -1;
	sys_write(1, s, strlen(s));
	sys_write(1, "\n", 1);
	return 0;
}

int printf(const char *fmt, ...)
{
	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	size_t len = (n < (int)sizeof(buf)) ? (size_t)n : (sizeof(buf) - 1);
	sys_write(1, buf, len);
	return n;
}
