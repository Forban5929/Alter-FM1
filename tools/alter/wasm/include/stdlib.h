#pragma once
#include <stddef.h>
void *malloc(size_t); void *calloc(size_t, size_t); void *realloc(void *, size_t); void free(void *);
void exit(int); void abort(void); int abs(int); long labs(long); int atoi(const char *); long strtol(const char *, char **, int);
unsigned long strtoul(const char *, char **, int); int rand(void); void srand(unsigned); char *getenv(const char *);
void qsort(void *, size_t, size_t, int (*)(const void *, const void *));
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
