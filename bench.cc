#include "port.h"

#include "bench.h"
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <future>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <unistd.h>

static void do_reads(std::poisson_distribution<uint32_t> dist, int32_t nops,
                     int fd, char *buf, size_t bufsiz,
                     std::promise<std::vector<int64_t>> latencies_promise);
static void do_writes(std::poisson_distribution<uint32_t> dist, int32_t nops,
                      int fd, char *buf, size_t bufsiz, size_t writesiz);

// bench computes the average read latency in us for the config.
// bench will create, load data into, and delete benchfile.
int64_t bench(const std::string &benchfile, BenchConfig cfg) {

  if (cfg.writesiz < cfg.wbufsiz) {
    std::cerr << "writesiz must be >= write buffer size\n";
    exit(125);
  }

  char *rbuf = (char *)aalloc(cfg.rbufsiz);
  char *wbuf = (char *)aalloc(cfg.wbufsiz);

  if (DEBUG)
    std::cerr << "# DEBUG: reopening " << benchfile << " in direct io mode\n";
  int fd = open(benchfile.c_str(), O_RDWR | O_DIRECT);
  if (fd == -1) {
    perror("unable to open data file");
    exit(66);
  }

  int32_t num_writes =
      ((double)cfg.num_reads / cfg.mean_reads_s) * cfg.mean_writes_s;
  double mean_us_read = 1e6 / cfg.mean_reads_s;
  double mean_us_write = 1e6 / cfg.mean_writes_s;

  std::poisson_distribution<uint32_t> read_dist(mean_us_read);
  std::poisson_distribution<uint32_t> write_dist(mean_us_write);

  std::promise<std::vector<int64_t>> read_latencies_promise;
  auto read_latencies_future = read_latencies_promise.get_future();

  std::thread read_thread(do_reads, std::move(read_dist), cfg.num_reads, fd,
                          rbuf, cfg.rbufsiz, std::move(read_latencies_promise));
  std::thread write_thread(do_writes, std::move(write_dist), num_writes, fd,
                           wbuf, cfg.wbufsiz, cfg.writesiz);

  read_thread.join();
  write_thread.join();

  std::vector<int64_t> read_latencies = read_latencies_future.get();

  double sum = 0;
  for (int64_t &l : read_latencies)
    sum += l;

  free(rbuf);
  free(wbuf);
  close(fd);

  return sum / (double)read_latencies.size();
}

static void do_reads(std::poisson_distribution<uint32_t> dist, int32_t nops,
                     int fd, char *buf, size_t bufsiz,
                     std::promise<std::vector<int64_t>> latencies_promise) {
  std::mt19937 gen(0);
  std::mt19937 tgen(0);
  std::vector<int64_t> latencies(nops);
  std::uniform_int_distribution<ssize_t> seek_dist;
  for (int32_t i = 0; i < nops; i++) {
    struct timespec start, end;
    off_t offset = seek_dist(gen) % BENCH_FILE_SIZ;
    offset /= bufsiz;
    offset *= bufsiz;
    monotonic_time(&start);
    if (lseek(fd, offset, SEEK_SET) != offset)
      perror("unable to seek");
    for (size_t sofar = 0; sofar < bufsiz;) {
      int got = read(fd, buf, bufsiz - sofar);
      if (got == -1)
        perror("unable to read data");
      sofar += (size_t)got;
    }
    monotonic_time(&end);
    int64_t latency = diff_time_us(start, end);
    latencies[i] = latency;
    int64_t sleep_us = std::max((int64_t)dist(tgen) - latency, (int64_t)0);
    usleep(sleep_us);
  }
  latencies_promise.set_value(std::move(latencies));
}

static void do_writes(std::poisson_distribution<uint32_t> dist, int32_t nops,
                      int fd, char *buf, size_t bufsiz, size_t writesiz) {
  std::mt19937 gen(0);
  std::mt19937 tgen(0);
  std::uniform_int_distribution<ssize_t> seek_dist;
  char *junk = new char[writesiz];

  for (int32_t i = 0; i < nops; i++) {
    struct timespec start, end;
    off_t offset = seek_dist(gen) % BENCH_FILE_SIZ;
    offset /= bufsiz;
    offset *= bufsiz;
    monotonic_time(&start);
    if (lseek(fd, offset, SEEK_SET) != offset)
      perror("unable to seek");
    for (size_t sofar = 0; sofar < writesiz;) {
      memmove(buf, junk + sofar, std::min(bufsiz, writesiz - sofar));
      int got = write(fd, buf, bufsiz);
      if (got == -1)
        perror("unable to write data");
      sofar += (size_t)got;
    }
    monotonic_time(&end);
    int64_t latency = diff_time_us(start, end);
    int64_t sleep_us = std::max((int64_t)dist(tgen) - latency, (int64_t)0);
    usleep(sleep_us);
  }

  delete junk;
}
