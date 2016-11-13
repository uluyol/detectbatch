#include <ctime>

void monotonic_time(struct timespec *ts) { clock_gettime(CLOCK_MONOTONIC, ts); }
