#include <ctime>
#include <windows.h>

// Taken from Asain Kujovic's response on
// http://stackoverflow.com/questions/5404277/porting-clock-gettime-to-windows
void monotonic_time(struct timespec *spec) {
  __int64 wintime;
  GetSystemTimeAsFileTime((FILETIME *)&wintime);
  spec->tv_sec = wintime / 10000000LL;
  spec->tv_nsec = wintime % 10000000LL;
}