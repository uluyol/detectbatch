#include <cstddef>
#include <cstdint>
#include <string>

typedef struct {
  double mean_reads_s;
  double mean_writes_s;
  size_t rbufsiz;
  size_t wbufsiz;
  size_t writesiz;
  int32_t num_reads;
} BenchConfig;

void load_file(const std::string &benchfile);
int64_t bench(const std::string &benchfile, BenchConfig cfg);
double bench_tput(const std::string &benchfile, int32_t nops, ssize_t bufsiz);
