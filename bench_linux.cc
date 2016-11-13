#include <cstdlib>
#include <unistd.h>

void *aalloc(size_t batchsize) {
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
