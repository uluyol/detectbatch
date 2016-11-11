#include "bench.h"
#include <algorithm>
#include <cstdbool>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

static std::string trim_right_num(std::string s);
static std::string devof(std::string path);

std::string resolve_link(std::string path);
int exec_getout(std::vector<char> *output, std::string cmd,
                std::vector<const char *> &args);
ssize_t readnum(std::string path);

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
  cfg.mean_reads_s = 50;
  cfg.mean_writes_s = 10;
  cfg.rbufsiz = base_io_size;
  cfg.writesiz = base_io_size * 128;
  cfg.num_reads = 1000;
  for (int64_t mul = 1; mul <= 10; mul++) {
    cfg.wbufsiz = base_io_size << mul;
    int64_t l = bench(tmpfile, cfg);
    avg_read_latencies.push_back(l);
    if (DEBUG)
      std::cerr << "# DEBUG: bufsiz: " << cfg.wbufsiz << " avg latency: " << l
                << "\n";
  }

  std::cout << base_io_size << "\n";

  remove(tmpfile.c_str());

  return 0;
}

static std::string devof(std::string path) {
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
