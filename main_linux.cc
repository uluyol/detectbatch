#include "bench.h"
#include <algorithm>
#include <cmath>
#include <cstdbool>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

static std::string trim_right_num(std::string s);
static std::string devof(const std::string &path);

std::string resolve_link(const std::string &path);
int exec_getout(std::vector<char> *output, const std::string &cmd,
                const std::vector<const char *> &args);
ssize_t readnum(const std::string &path);

static ssize_t search_io_size(ssize_t base_io_size, int64_t maxshift,
                              BenchConfig cfg, const std::string &tmpfile);

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: detect_batch /path/to/directory\n";
    exit(88);
  }
  std::string dir_path(argv[1]);
  auto dev_path = devof(dir_path);
  if (DEBUG) {
    std::cerr << "# DEBUG: dir_path: " << dir_path << "\n";
    std::cerr << "# DEBUG: dev_path: " << dev_path << "\n";
  }
  dev_path = resolve_link(dev_path);
  if (DEBUG)
    std::cerr << "# DEBUG: resolved: " << dev_path << "\n";
  auto last = dev_path.rfind("/");
  if (last == std::string::npos) {
    // not a device, just give up
    if (DEBUG)
      std::cerr << "# DEBUG: unable to find /, not a real device\n";
    std::cout << "512\n";
    return 10;
  }

  // try both device name and try stripping trailing nums.
  // /dev/sdX could be the block device or /dev/sdXN could be it.
  // device-mapper and friends make things more complicated
  auto dev_name1 = dev_path.substr(last + 1, dev_path.size() - last);
  auto dev_name2 = trim_right_num(dev_name1);

  std::vector<std::string> sysfs_paths = {
      "/sys/block/" + dev_name1 + "/queue/minimum_io_size",
      "/sys/block/" + dev_name2 + "/queue/minimum_io_size",
      "/sys/block/" + dev_name1 + "/queue/physical_block_size",
      "/sys/block/" + dev_name2 + "/queue/physical_block_size",
      "/sys/block/" + dev_name1 + "/queue/logical_block_size",
      "/sys/block/" + dev_name2 + "/queue/logical_block_size",
  };

  std::vector<size_t> sysfs_nums;

  ssize_t base_io_size = 512;

  for (auto &p : sysfs_paths) {
    auto v = readnum(p);
    sysfs_nums.push_back(v);
    base_io_size = std::max(base_io_size, v);
  }

  if (DEBUG) {
    std::cerr << "# DEBUG: dev_name1: " << dev_name1 << "\n";
    std::cerr << "# DEBUG: dev_name2: " << dev_name2 << "\n";
    for (size_t i = 0; i < sysfs_paths.size(); i++)
      std::cerr << "# DEBUG: " << sysfs_paths[i] << ": " << sysfs_nums[i]
                << "\n";
  }

  std::string tmpfile = dir_path + "/.detectbatch.tmp";

  load_file(tmpfile);

  std::vector<int64_t> avg_read_latencies;
  BenchConfig cfg;
  cfg.mean_reads_s = 80;
  cfg.mean_writes_s = 5;
  cfg.rbufsiz = base_io_size;
  cfg.writesiz = 10 * (1 << 20); // 10 MB
  cfg.num_reads = 1000;

  int64_t io_size = search_io_size(base_io_size, 8, cfg, tmpfile);

  std::cout << io_size << "\n";

  remove(tmpfile.c_str());

  return 0;
}

// golden ratio
static double phi = (sqrt(5.0) + 1) / 2;

// golden_section_search looks for a minimum between a and b.
//
// Algorithm from
// https://ece.uwaterloo.ca/~dwharder/NumericalAnalysis/11Optimization/golden/
//
static ssize_t golden_section_search(ssize_t a, ssize_t b, ssize_t base_io_size,
                                     BenchConfig cfg,
                                     const std::string &tmpfile) {
  ssize_t c = (phi - 1) * a + (2 - phi) * b;
  c /= base_io_size;
  c *= base_io_size;
  ssize_t d = (2 - phi) * a + (phi - 1) * b;
  d /= base_io_size;
  d *= base_io_size;

  bool do_fc = true;
  bool do_fd = true;
  int64_t fd = 0;
  int64_t fc = 0;

  ssize_t epsilon = 3 * base_io_size;

  while ((b - a) > epsilon && c != d) {
    if (do_fc) {
      cfg.wbufsiz = c;
      fc = bench(tmpfile, cfg);
      do_fc = false;
    }
    if (do_fd) {
      cfg.wbufsiz = d;
      fd = bench(tmpfile, cfg);
      do_fd = false;
    }

    if (DEBUG) {
      std::cerr << "# DEBUG: a: " << a << " b: " << b << "\n";
      std::cerr << "# DEBUG: bufsiz c: " << c << " avg latency: " << fc << "\n";
      std::cerr << "# DEBUG: bufsiz d: " << d << " avg latency: " << fd << "\n";
    }

    if (fc < fd) {
      b = d;
      d = c;
      fd = fc;
      c = (phi - 1) * a + (2 - phi) * b;
      c /= base_io_size;
      c *= base_io_size;
      do_fc = true;
    } else {
      a = c;
      c = d;
      fc = fd;
      d = (2 - phi) * a + (phi - 1) * b;
      d /= base_io_size;
      d *= base_io_size;
      do_fd = true;
    }
  }
  ssize_t result = (a + b) / 2;
  result /= base_io_size;
  result *= base_io_size;
  return result;
}

static ssize_t search_io_size(ssize_t base_io_size, int64_t maxshift,
                              BenchConfig cfg, const std::string &tmpfile) {
  // Use at most 300 reads here since we increase in powers of two.
  // Be more careful with binary search.
  int32_t orig_num_reads = cfg.num_reads;
  cfg.num_reads = std::min(orig_num_reads, (int32_t)300);
  int64_t prev_latency = INT64_MAX;
  for (int64_t shift = 0; shift <= maxshift; shift++) {
    cfg.wbufsiz = base_io_size << shift;
    int64_t l = bench(tmpfile, cfg);
    if (DEBUG) {
      std::cerr << "# DEBUG: bufsiz: " << cfg.wbufsiz << " avg latency: " << l
                << "\n";
    }
    if (l <= prev_latency) {
      prev_latency = l;
    } else if (l > prev_latency) {
      cfg.num_reads = orig_num_reads;
      // search between 5 shift values instead of 3 for better accuracy
      int64_t left_shift = std::max((int64_t)0, shift - 3);
      return golden_section_search(base_io_size << left_shift,
                                   base_io_size << (shift + 1), base_io_size,
                                   cfg, tmpfile);
    }
  }
  // We only get here if latencies never begin to increase.
  // Return max attempted buffer size.
  return cfg.wbufsiz;
}

static std::string devof(const std::string &path) {
  std::vector<char> out;

  std::vector<const char *> args = {path.c_str()};
  int ret = exec_getout(&out, "df", args);
  if (ret != 0) {
    std::cerr << "unable to run df\n";
    exit(5);
  }

  bool found_end = false;
  int start_index = 0;
  int stop_index = 0;
  for (size_t i = 0; i < out.size(); i++) {
    if (found_end && isspace(out[i])) {
      stop_index = i;
      break;
    } else {
      if (out[i] == '\n') {
        found_end = true;
        start_index = i + 1;
      }
    }
  }

  if (start_index >= stop_index) {
    return "";
  }

  std::string dev(&out[start_index], stop_index - start_index);
  return dev;
}

static std::string trim_right_num(std::string s) {
  while (s.size() > 0) {
    if (s.back() < '0' || s.back() > '9')
      break;
    s.pop_back();
  }
  return s;
}
