/* SPDX-License-Identifier: GPL-3.0-only
 * A minimal libc for the browser build with the system clang (tools/alter/wasm/build_emu.sh): just the
 * declarations the firmware and tests/hostsim.c name; what the page reaches is in libc.c. */
#pragma once
#include <stddef.h>
#include <stdarg.h>
typedef struct FILE FILE;
extern FILE *stdout, *stderr;
int printf(const char *, ...);
int fprintf(FILE *, const char *, ...);
int snprintf(char *, size_t, const char *, ...);
int vsnprintf(char *, size_t, const char *, va_list);
int sprintf(char *, const char *, ...);
int sscanf(const char *, const char *, ...);
int remove(const char *);
int puts(const char *);
int putchar(int);
int fputs(const char *, FILE *);
int fputc(int, FILE *);
int fflush(FILE *);
FILE *fopen(const char *, const char *);
int fclose(FILE *);
size_t fwrite(const void *, size_t, size_t, FILE *);
size_t fread(void *, size_t, size_t, FILE *);
int fseek(FILE *, long, int);
long ftell(FILE *);
#define SEEK_SET 0
#define SEEK_END 2
