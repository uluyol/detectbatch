#include <ctime>
#include <mach/clock.h>
#include <mach/mach.h>

// Taken from https://gist.github.com/jbenet/1087739
void monotonic_time(struct timespec *ts) {
  clock_serv_t cclock;
  mach_timespec_t mts;
  host_get_clock_service(mach_host_self(), REALTIME_CLOCK, &cclock);
  clock_get_time(cclock, &mts);
  mach_port_deallocate(mach_task_self(), cclock);
  ts->tv_sec = mts.tv_sec;
  ts->tv_nsec = mts.tv_nsec;
}
