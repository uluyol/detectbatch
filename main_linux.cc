#include <algorithm>
#include <cstdbool>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

static std::string trim_right_num(std::string s);
static std::string devof(std::string path);
static void *aalloc(size_t batchsize);

int exec_getout(std::vector<char> *output, std::string cmd,
                std::vector<const char *> &args);
ssize_t readnum(std::string path);

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: detect_batch /path/to/directory\n");
    exit(88);
  }
  std::string dir_path(argv[1]);
  auto dev_path = devof(dir_path);
  if (DEBUG) {
    std::cerr << "# DEBUG: dir_path: " << dir_path << "\n";
    std::cerr << "# DEBUG: dev_path: " << dev_path << "\n";
  }
  auto last = dev_path.rfind("/");
  if (last == std::string::npos) {
    // not a device, just give up
    if (DEBUG)
      std::cerr << "# DEBUG: unable to find /, not a real device\n";
    puts("512");
    return 10;
  }
  auto dev_name =
      trim_right_num(dev_path.substr(last + 1, dev_path.size() - last));

  auto min_io_size =
      readnum("/sys/block/" + dev_name + "/queue/minimum_io_size");
  auto phy_block_size =
      readnum("/sys/block/" + dev_name + "/queue/physical_block_size");
  auto log_block_size =
      readnum("/sys/block/" + dev_name + "/queue/logical_block_size");

  ssize_t base_io_size =
      std::max((ssize_t)512,
               std::max(min_io_size, std::max(phy_block_size, log_block_size)));

  if (DEBUG) {
    std::cerr << "# DEBUG: dev_name: " << dev_name << "\n";
    std::cerr << "# DEBUG: minimum_io_size: " << min_io_size << "\n";
    std::cerr << "# DEBUG: physical_block_size: " << phy_block_size << "\n";
    std::cerr << "# DEBUG: logical_block_size: " << log_block_size << "\n";
    std::cerr << "# DEBUG: base_io_size: " << base_io_size << "\n";
  }

  std::cout << base_io_size << "\n";
  return 0;
}

static std::string devof(std::string path) {
  std::vector<char> out;

  std::vector<const char *> args = {path.c_str()};
  int ret = exec_getout(&out, "df", args);
  if (ret != 0) {
    fprintf(stderr, "unable to run df\n");
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

static void *aalloc(size_t batchsize) {
  void *ptr;

  size_t siz = 1;
  while (siz < batchsize) {
    siz *= 2;
  }

  if (posix_memalign(&ptr, siz, siz) != 0) {
    return NULL;
  }
  return ptr;
}

static std::string trim_right_num(std::string s) {
  while (s.size() > 0) {
    if (s.back() < '0' || s.back() > '9')
      break;
    s.pop_back();
  }
  return s;
}
