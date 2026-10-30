#pragma once
typedef long clock_t; typedef long long time_t;
#define CLOCKS_PER_SEC 1000000L
clock_t clock(void); time_t time(time_t *);
struct timespec { long long tv_sec; long tv_nsec; };
#define CLOCK_MONOTONIC 1
int clock_gettime(int, struct timespec *);
