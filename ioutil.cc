#include <fstream>
#include <iterator>
#include <limits.h>
#include <stdexcept>
#include <stdint.h>
#include <streambuf>
#include <string>
#include <unistd.h>

std::string readall(std::string path) {
  std::ifstream t;

  try {
    t.open(path);
    std::string body((std::istreambuf_iterator<char>(t)),
                     std::istreambuf_iterator<char>());
    return body;
  } catch (...) {
    return "";
  }
}

ssize_t readnum(std::string path) {
  auto body = readall(path);
  try {
    return (ssize_t)std::stoul(body);
  } catch (const std::invalid_argument &e) {
    return 0;
  }
}

std::string resolve_link(std::string path) {
  char buf[PATH_MAX];
  auto len = readlink(path.c_str(), buf, PATH_MAX);
  if (len == -1) {
    return path;
  }
  std::string resolved(buf, len);
  return resolve_link(resolved);
}
