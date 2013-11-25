#pragma once

#include <malloc.h>
#include <new>

namespace ebbrt {

const constexpr size_t cache_size = 64;

struct alignas(cache_size) cache_aligned{
  void *operator new(size_t size) { auto ret = memalign(cache_size, size);
    if (ret == nullptr) {
      throw std::bad_alloc();
    }
    return ret;
  }
  void operator delete(void *p) { free(p); }
}
;
}
