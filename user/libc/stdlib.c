#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "syscall.h"

struct alloc_hdr {
	size_t size;
	size_t _pad; /* Keep returned pointer 16-byte aligned for SSE. */
};

void *malloc(size_t size)
{
	if (size == 0) size = 1;
	size = (size + 15U) & ~15U;
	struct alloc_hdr *h = (struct alloc_hdr *)sys_alloc(sizeof(*h) + size);
	if (!h) return NULL;
	h->size = size;
	return (void *)(h + 1);
}

void *calloc(size_t nmemb, size_t size)
{
	size_t total = nmemb * size;
	void *p = malloc(total);
	if (p) memset(p, 0, total);
	return p;
}

void *realloc(void *ptr, size_t size)
{
	if (!ptr) return malloc(size);
	if (size == 0) { free(ptr); return NULL; }
	struct alloc_hdr *h = ((struct alloc_hdr *)ptr) - 1;
	if (size <= h->size) return ptr;
	void *np = malloc(size);
	if (!np) return NULL;
	memcpy(np, ptr, h->size);
	return np;
}

void free(void *ptr)
{
	(void)ptr; /* Bump allocator backed by sys_alloc; process memory is reclaimed on exit. */
}

int atoi(const char *s)
{
	if (!s) return 0;
	while (*s == ' ' || *s == '\t' || *s == '\n') s++;
	int sign = 1;
	if (*s == '-') { sign = -1; s++; }
	else if (*s == '+') { s++; }
	int v = 0;
	while (*s >= '0' && *s <= '9') {
		v = v * 10 + (*s - '0');
		s++;
	}
	return sign * v;
}

int abs(int x)
{
	return (x < 0) ? -x : x;
}

static uint32_t s_rng = 1;

void srand(unsigned int seed)
{
	s_rng = seed ? (uint32_t)seed : 1U;
}

int rand(void)
{
	s_rng = s_rng * 1103515245U + 12345U;
	return (int)((s_rng >> 16) & 0x7FFFU);
}

void exit(int status)
{
	sys_exit(status);
	for (;;) {}
}
