#ifdef __linux__
#ifndef _GNU_SOURCE
#define _GNU_SOURCE true
#endif
#define __STDC_LIMIT_MACROS
#else
#define O_DIRECT 0
#endif

#include <ctime>

void monotonic_time(struct timespec *ts);
void *aalloc(size_t batchsize);
