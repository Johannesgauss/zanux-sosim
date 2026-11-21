#ifndef ZANUX_LIBC_STDLIB_H
#define ZANUX_LIBC_STDLIB_H

#include <stddef.h>

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void  free(void *ptr);

int   atoi(const char *s);
int   abs(int x);
void  srand(unsigned int seed);
int   rand(void);
void  exit(int status) __attribute__((noreturn));

#endif // ZANUX_LIBC_STDLIB_H
