#include "bench.h"
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <unistd.h>

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: detect_batch /path/to/directory\n";
    exit(88);
  }
  std::string dir_path(argv[1]);

  if (DEBUG) {
    std::cerr << "# DEBUG: dir_path: " << dir_path << "\n";
  }

  std::string tmpfile = dir_path + "/.detectbatch.tmp";

  load_file(tmpfile);
  int64_t io_size = 16 * 1024;
  double tput = bench_tput(tmpfile, 1000, io_size);
  std::cout << io_size << " " << tput << "\n";

  remove(tmpfile.c_str());

  return 0;
}
