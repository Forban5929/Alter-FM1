#pragma once
#include <unistd.h>
pid_t wait(int *);
pid_t waitpid(pid_t, int *, int);
#define WEXITSTATUS(s) (((s) >> 8) & 255)
#define WIFEXITED(s) 1
#define WIFSIGNALED(s) 0
#define WTERMSIG(s) 0
