#include <fstream>
#include <iterator>
#include <stdexcept>
#include <stdint.h>
#include <streambuf>
#include <string>

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
