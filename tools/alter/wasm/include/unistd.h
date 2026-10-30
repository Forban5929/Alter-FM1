#pragma once
typedef int pid_t;
pid_t fork(void);
int usleep(unsigned);
int close(int);
int _exit(int);
int pipe(int *);
long write(int, const void *, unsigned long);
long read(int, void *, unsigned long);
