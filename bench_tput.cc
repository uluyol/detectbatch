#include "port.h"

#include "bench.h"
#include <cstdint>
#include <fcntl.h>
#include <iostream>
#include <random>
#include <string>
#include <unistd.h>

int64_t diff_time_us(struct timespec start, struct timespec end) {
  int64_t sec_diff = end.tv_sec - start.tv_sec;
  int64_t ns_diff = end.tv_nsec - start.tv_nsec;
  return (int64_t)(sec_diff * 1000000L + ns_diff / 1000L);
}

void load_file(const std::string &p) {
  size_t bufsiz = 1024 * 1024;
  char *buf = new char[bufsiz];

  if (DEBUG)
    std::cerr << "# DEBUG: writing random data to " << p << "\n";

  int wfd = creat(p.c_str(), 0666);
  if (wfd == -1) {
    perror("unable to create temp file");
    exit(123);
  }
  int fdrand = open("/dev/urandom", O_RDONLY);

  ssize_t wrote = 0;
  while (wrote < BENCH_FILE_SIZ) {
    ssize_t got = read(fdrand, buf, bufsiz);
    if (got == -1) {
      perror("unable to read /dev/urandom");
      continue;
    }
    wrote += write(wfd, buf, got);
  }

  if (DEBUG)
    std::cerr << "# DEBUG: wrote " << wrote << " bytes\n";

  close(wfd);
  close(fdrand);
  delete buf;
}

double bench_tput(const std::string &benchfile, int32_t nops, ssize_t bufsiz) {
  char *buf = (char *)aalloc(bufsiz);

  if (DEBUG)
    std::cerr << "# DEBUG: reopening " << benchfile << " in direct io mode\n";
  int fd = open(benchfile.c_str(), O_RDWR | O_DIRECT);
  if (fd == -1) {
    perror("unable to open data file");
    exit(66);
  }

  std::mt19937 gen(0);
  std::mt19937 tgen(0);
  std::uniform_int_distribution<ssize_t> seek_dist;

  struct timespec start, end;
  monotonic_time(&start);

  for (int32_t i = 0; i < nops; i++) {
    off_t offset = seek_dist(gen) % BENCH_FILE_SIZ;
    offset /= bufsiz;
    offset *= bufsiz;
    if (lseek(fd, offset, SEEK_SET) != offset)
      perror("unable to seek");
    for (size_t sofar = 0; sofar < (size_t)bufsiz;) {
      int got = write(fd, buf, bufsiz);
      if (got == -1)
        perror("unable to write data");
      sofar += (size_t)got;
    }
  }

  monotonic_time(&end);

  free(buf);
  close(fd);

  double rt_sec = diff_time_us(start, end) / 1e6;
  return (double)nops / rt_sec;
}
