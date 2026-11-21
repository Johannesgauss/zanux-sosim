#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <sys/stat.h>
#include <stdbool.h>
#include "syscall.h"

int errno = 0;

static FILE s_stdio_streams[3];
FILE *stdin  = &s_stdio_streams[0];
FILE *stdout = &s_stdio_streams[1];
FILE *stderr = &s_stdio_streams[2];

struct alloc_hdr {
	size_t size;
	uint64_t magic;
};

#define ALLOC_MAGIC 0xD009CAFE12345678ULL

static void serial_out_char(char c)
{
	sys_write(1, &c, 1);
}

static void serial_out_str(const char *s)
{
	if (!s)
		return;
	while (*s) {
		serial_out_char(*s++);
	}
}

void *malloc(size_t size)
{
	if (size == 0)
		size = 8;
	size_t total = size + sizeof(struct alloc_hdr);
	struct alloc_hdr *hdr = (struct alloc_hdr *)sys_alloc(total);
	if (!hdr)
		return NULL;
	hdr->size = size;
	hdr->magic = ALLOC_MAGIC;
	return (void *)(hdr + 1);
}

void free(void *ptr)
{
	(void)ptr;
}

void *calloc(size_t nmemb, size_t size)
{
	size_t total = nmemb * size;
	void *p = malloc(total);
	if (p)
		memset(p, 0, total);
	return p;
}

void *realloc(void *ptr, size_t size)
{
	if (!ptr)
		return malloc(size);
	if (size == 0)
		return NULL;

	struct alloc_hdr *hdr = ((struct alloc_hdr *)ptr) - 1;
	size_t old_size = (hdr->magic == ALLOC_MAGIC) ? hdr->size : size;
	if (size <= old_size)
		return ptr;

	void *new_ptr = malloc(size);
	if (new_ptr) {
		memcpy(new_ptr, ptr, old_size);
	}
	return new_ptr;
}

size_t strlen(const char *s)
{
	size_t len = 0;
	if (!s)
		return 0;
	while (s[len])
		len++;
	return len;
}

char *strcpy(char *dest, const char *src)
{
	char *d = dest;
	while ((*d++ = *src++) != '\0') {}
	return dest;
}

char *strncpy(char *dest, const char *src, size_t n)
{
	size_t i = 0;
	for (; i < n && src[i] != '\0'; i++)
		dest[i] = src[i];
	for (; i < n; i++)
		dest[i] = '\0';
	return dest;
}

char *strcat(char *dest, const char *src)
{
	char *d = dest + strlen(dest);
	while ((*d++ = *src++) != '\0') {}
	return dest;
}

char *strncat(char *dest, const char *src, size_t n)
{
	char *d = dest + strlen(dest);
	size_t i = 0;
	for (; i < n && src[i] != '\0'; i++)
		d[i] = src[i];
	d[i] = '\0';
	return dest;
}

int strcmp(const char *s1, const char *s2)
{
	while (*s1 && (*s1 == *s2)) {
		s1++;
		s2++;
	}
	return (unsigned char)*s1 - (unsigned char)*s2;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		unsigned char c1 = (unsigned char)s1[i];
		unsigned char c2 = (unsigned char)s2[i];
		if (c1 != c2)
			return c1 - c2;
		if (c1 == '\0')
			return 0;
	}
	return 0;
}

int strcasecmp(const char *s1, const char *s2)
{
	while (*s1 && (tolower((unsigned char)*s1) == tolower((unsigned char)*s2))) {
		s1++;
		s2++;
	}
	return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

int strncasecmp(const char *s1, const char *s2, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		int c1 = tolower((unsigned char)s1[i]);
		int c2 = tolower((unsigned char)s2[i]);
		if (c1 != c2)
			return c1 - c2;
		if (c1 == '\0')
			return 0;
	}
	return 0;
}

char *strchr(const char *s, int c)
{
	while (*s) {
		if (*s == (char)c)
			return (char *)s;
		s++;
	}
	if ((char)c == '\0')
		return (char *)s;
	return NULL;
}

char *strrchr(const char *s, int c)
{
	const char *last = NULL;
	while (*s) {
		if (*s == (char)c)
			last = s;
		s++;
	}
	if ((char)c == '\0')
		return (char *)s;
	return (char *)last;
}

char *strstr(const char *haystack, const char *needle)
{
	if (!*needle)
		return (char *)haystack;
	size_t nlen = strlen(needle);
	while (*haystack) {
		if (*haystack == *needle && strncmp(haystack, needle, nlen) == 0)
			return (char *)haystack;
		haystack++;
	}
	return NULL;
}

char *strdup(const char *s)
{
	size_t len = strlen(s) + 1;
	char *copy = (char *)malloc(len);
	if (copy)
		memcpy(copy, s, len);
	return copy;
}

void *memchr(const void *s, int c, size_t n)
{
	const unsigned char *p = (const unsigned char *)s;
	for (size_t i = 0; i < n; i++) {
		if (p[i] == (unsigned char)c)
			return (void *)(p + i);
	}
	return NULL;
}

int abs(int j)
{
	return (j < 0) ? -j : j;
}

long labs(long j)
{
	return (j < 0) ? -j : j;
}

long strtol(const char *nptr, char **endptr, int base)
{
	const char *s = nptr;
	while (isspace((unsigned char)*s))
		s++;

	int neg = 0;
	if (*s == '-') {
		neg = 1;
		s++;
	} else if (*s == '+') {
		s++;
	}

	if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
		s += 2;
		base = 16;
	} else if (base == 0) {
		base = (s[0] == '0') ? 8 : 10;
	}

	long acc = 0;
	while (*s) {
		int digit;
		unsigned char c = (unsigned char)*s;
		if (isdigit(c))
			digit = c - '0';
		else if (isalpha(c))
			digit = tolower(c) - 'a' + 10;
		else
			break;
		if (digit >= base)
			break;
		acc = acc * base + digit;
		s++;
	}

	if (endptr)
		*endptr = (char *)s;
	return neg ? -acc : acc;
}

unsigned long strtoul(const char *nptr, char **endptr, int base)
{
	return (unsigned long)strtol(nptr, endptr, base);
}

int atoi(const char *nptr)
{
	return (int)strtol(nptr, NULL, 10);
}

long atol(const char *nptr)
{
	return strtol(nptr, NULL, 10);
}

int atof(const char *nptr)
{
	return atoi(nptr);
}

static unsigned int s_rand_seed = 1;
int rand(void)
{
	s_rand_seed = s_rand_seed * 1103515245U + 12345U;
	return (int)((s_rand_seed >> 16) & 0x7fff);
}

void srand(unsigned int seed)
{
	s_rand_seed = seed;
}

int system(const char *command)
{
	(void)command;
	return -1;
}

char *getenv(const char *name)
{
	(void)name;
	return NULL;
}

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *))
{
	if (nmemb < 2 || size == 0)
		return;
	uint8_t *arr = (uint8_t *)base;
	uint8_t tmp[256];
	for (size_t i = 1; i < nmemb; i++) {
		size_t j = i;
		while (j > 0 && compar(arr + (j - 1) * size, arr + j * size) > 0) {
			uint8_t *a = arr + (j - 1) * size;
			uint8_t *b = arr + j * size;
			for (size_t k = 0; k < size; k++) {
				uint8_t t = a[k];
				a[k] = b[k];
				b[k] = t;
			}
			(void)tmp;
			j--;
		}
	}
}

static uint8_t *s_wad_buf = NULL;
static size_t s_wad_size = 0;

static bool get_wad_file(const char *path, const uint8_t **out_data, size_t *out_size)
{
	if (!s_wad_buf) {
		int64_t sz = sys_read_file(path, NULL, 0);
		if (sz <= 0)
			return false;
		s_wad_buf = (uint8_t *)sys_alloc((size_t)sz);
		if (!s_wad_buf)
			return false;
		if (sys_read_file(path, s_wad_buf, (size_t)sz) <= 0)
			return false;
		s_wad_size = (size_t)sz;
	}
	*out_data = s_wad_buf;
	*out_size = s_wad_size;
	return true;
}

static bool is_wad_path(const char *path)
{
	if (!path)
		return false;
	size_t len = strlen(path);
	if (len >= 4 && strcasecmp(path + len - 4, ".wad") == 0)
		return true;
	return false;
}

FILE *fopen(const char *path, const char *mode)
{
	if (!path || !mode)
		return NULL;
	if (mode[0] == 'w' || mode[0] == 'a')
		return NULL;

	if (is_wad_path(path)) {
		const uint8_t *data = NULL;
		size_t size = 0;
		if (!get_wad_file(path, &data, &size))
			return NULL;
		FILE *f = (FILE *)malloc(sizeof(FILE));
		if (!f)
			return NULL;
		f->data = data;
		f->size = size;
		f->pos = 0;
		f->is_wad = 1;
		return f;
	}
	return NULL;
}

int fclose(FILE *stream)
{
	(void)stream;
	return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	if (!ptr || !stream || !stream->data || size == 0 || nmemb == 0)
		return 0;
	size_t avail = (stream->pos < stream->size) ? (stream->size - stream->pos) : 0;
	size_t total_req = size * nmemb;
	if (total_req > avail)
		total_req = (avail / size) * size;
	if (total_req > 0) {
		memcpy(ptr, stream->data + stream->pos, total_req);
		stream->pos += total_req;
	}
	return total_req / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	(void)ptr;
	(void)stream;
	return nmemb * size;
}

int fseek(FILE *stream, long offset, int whence)
{
	if (!stream)
		return -1;
	long target = 0;
	if (whence == SEEK_SET)
		target = offset;
	else if (whence == SEEK_CUR)
		target = (long)stream->pos + offset;
	else if (whence == SEEK_END)
		target = (long)stream->size + offset;
	else
		return -1;

	if (target < 0)
		target = 0;
	if ((size_t)target > stream->size)
		target = (long)stream->size;
	stream->pos = (size_t)target;
	return 0;
}

long ftell(FILE *stream)
{
	if (!stream)
		return -1;
	return (long)stream->pos;
}

int feof(FILE *stream)
{
	if (!stream)
		return 1;
	return (stream->pos >= stream->size) ? 1 : 0;
}

int fflush(FILE *stream)
{
	(void)stream;
	return 0;
}

int remove(const char *pathname)
{
	(void)pathname;
	return 0;
}

int rename(const char *oldpath, const char *newpath)
{
	(void)oldpath;
	(void)newpath;
	return 0;
}

int mkdir(const char *pathname, mode_t mode)
{
	(void)pathname;
	(void)mode;
	return 0;
}

int stat(const char *pathname, struct stat *statbuf)
{
	if (is_wad_path(pathname)) {
		const uint8_t *data = NULL;
		size_t size = 0;
		if (get_wad_file(pathname, &data, &size)) {
			if (statbuf) {
				statbuf->st_mode = 0;
				statbuf->st_size = (int64_t)size;
			}
			return 0;
		}
	}
	return -1;
}

int access(const char *pathname, int mode)
{
	(void)mode;
	if (is_wad_path(pathname))
		return 0;
	return -1;
}

static void append_char(char *buf, size_t size, size_t *idx, char c)
{
	if (buf && size > 0 && *idx + 1 < size) {
		buf[*idx] = c;
	}
	(*idx)++;
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
	size_t idx = 0;
	const char *p = format;

	while (p && *p) {
		if (*p != '%') {
			append_char(str, size, &idx, *p++);
			continue;
		}
		p++;

		char pad_char = ' ';
		int left_align = 0;
		if (*p == '-') {
			left_align = 1;
			p++;
		}
		if (*p == '0') {
			pad_char = '0';
			p++;
		}

		int width = 0;
		if (*p == '*') {
			width = va_arg(ap, int);
			p++;
		} else {
			while (isdigit((unsigned char)*p)) {
				width = width * 10 + (*p - '0');
				p++;
			}
		}

		int prec = -1;
		if (*p == '.') {
			p++;
			prec = 0;
			if (*p == '*') {
				prec = va_arg(ap, int);
				p++;
			} else {
				while (isdigit((unsigned char)*p)) {
					prec = prec * 10 + (*p - '0');
					p++;
				}
			}
		}

		int is_long = 0;
		while (*p == 'l' || *p == 'z' || *p == 'h') {
			if (*p == 'l' || *p == 'z')
				is_long = 1;
			p++;
		}

		char spec = *p ? *p++ : '\0';
		if (spec == '\0')
			break;

		if (spec == '%') {
			append_char(str, size, &idx, '%');
		} else if (spec == 'c') {
			char c = (char)va_arg(ap, int);
			append_char(str, size, &idx, c);
		} else if (spec == 's') {
			const char *s = va_arg(ap, const char *);
			if (!s)
				s = "(null)";
			int slen = (int)strlen(s);
			if (prec >= 0 && prec < slen)
				slen = prec;
			if (!left_align) {
				for (int i = slen; i < width; i++)
					append_char(str, size, &idx, ' ');
			}
			for (int i = 0; i < slen; i++)
				append_char(str, size, &idx, s[i]);
			if (left_align) {
				for (int i = slen; i < width; i++)
					append_char(str, size, &idx, ' ');
			}
		} else if (spec == 'd' || spec == 'i' || spec == 'u' || spec == 'x' || spec == 'X' || spec == 'p' || spec == 'o') {
			uint64_t uval;
			int neg = 0;
			int base = 10;
			int upper = (spec == 'X');

			if (spec == 'p') {
				uval = (uint64_t)(uintptr_t)va_arg(ap, void *);
				base = 16;
				append_char(str, size, &idx, '0');
				append_char(str, size, &idx, 'x');
			} else if (spec == 'd' || spec == 'i') {
				int64_t sval = is_long ? va_arg(ap, long) : va_arg(ap, int);
				if (sval < 0) {
					neg = 1;
					uval = (uint64_t)(-sval);
				} else {
					uval = (uint64_t)sval;
				}
			} else {
				uval = is_long ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
				if (spec == 'x' || spec == 'X')
					base = 16;
				else if (spec == 'o')
					base = 8;
			}

			char nbuf[32];
			int npos = 0;
			const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
			if (uval == 0) {
				nbuf[npos++] = '0';
			} else {
				while (uval > 0 && npos < 30) {
					nbuf[npos++] = digits[uval % (uint64_t)base];
					uval /= (uint64_t)base;
				}
			}

			int min_digits = (prec > npos) ? prec : npos;
			int total_len = min_digits + (neg ? 1 : 0);

			if (!left_align && pad_char == ' ') {
				for (int i = total_len; i < width; i++)
					append_char(str, size, &idx, ' ');
			}
			if (neg)
				append_char(str, size, &idx, '-');
			if (!left_align && pad_char == '0') {
				for (int i = total_len; i < width; i++)
					append_char(str, size, &idx, '0');
			}
			for (int i = npos; i < min_digits; i++)
				append_char(str, size, &idx, '0');
			while (npos > 0)
				append_char(str, size, &idx, nbuf[--npos]);
			if (left_align) {
				for (int i = total_len; i < width; i++)
					append_char(str, size, &idx, ' ');
			}
		} else {
			append_char(str, size, &idx, spec);
		}
	}

	if (str && size > 0) {
		size_t term = (idx < size) ? idx : (size - 1);
		str[term] = '\0';
	}
	return (int)idx;
}

int snprintf(char *str, size_t size, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	int r = vsnprintf(str, size, format, ap);
	va_end(ap);
	return r;
}

int sprintf(char *str, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	int r = vsnprintf(str, 4096, format, ap);
	va_end(ap);
	return r;
}

int vfprintf(FILE *stream, const char *format, va_list ap)
{
	(void)stream;
	char buf[512];
	int r = vsnprintf(buf, sizeof(buf), format, ap);
	serial_out_str(buf);
	return r;
}

int fprintf(FILE *stream, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	int r = vfprintf(stream, format, ap);
	va_end(ap);
	return r;
}

int printf(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	int r = vfprintf(stdout, format, ap);
	va_end(ap);
	return r;
}

int putchar(int c)
{
	serial_out_char((char)c);
	return c;
}

int putc(int c, FILE *stream)
{
	(void)stream;
	serial_out_char((char)c);
	return c;
}

int fputc(int c, FILE *stream)
{
	(void)stream;
	serial_out_char((char)c);
	return c;
}

int puts(const char *s)
{
	serial_out_str(s);
	serial_out_char('\n');
	return 0;
}

int sscanf(const char *str, const char *format, ...)
{
	if (!str || !format)
		return 0;
	va_list ap;
	va_start(ap, format);
	int matched = 0;
	const char *s = str;
	const char *f = format;

	while (*f && *s) {
		if (isspace((unsigned char)*f)) {
			while (isspace((unsigned char)*f))
				f++;
			while (isspace((unsigned char)*s))
				s++;
			continue;
		}
		if (*f != '%') {
			if (*f != *s)
				break;
			f++;
			s++;
			continue;
		}
		f++;
		if (*f == 'd' || *f == 'i' || *f == 'x' || *f == 'o') {
			int base = (*f == 'x') ? 16 : ((*f == 'o') ? 8 : ((*f == 'i') ? 0 : 10));
			char *end = NULL;
			long val = strtol(s, &end, base);
			if (end == s)
				break;
			int *out = va_arg(ap, int *);
			if (out)
				*out = (int)val;
			s = end;
			f++;
			matched++;
		} else if (*f == 's') {
			char *out = va_arg(ap, char *);
			while (isspace((unsigned char)*s))
				s++;
			if (!*s)
				break;
			while (*s && !isspace((unsigned char)*s)) {
				if (out)
					*out++ = *s;
				s++;
			}
			if (out)
				*out = '\0';
			f++;
			matched++;
		} else {
			break;
		}
	}
	va_end(ap);
	return matched;
}

int fscanf(FILE *stream, const char *format, ...)
{
	(void)stream;
	(void)format;
	return 0;
}
