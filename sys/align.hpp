#pragma once

#include <cstdint>
#include <cstring>

namespace ebbrt {
template <typename T, typename S> constexpr T align_down(T val, S alignment) {
  return val & ~(alignment - 1);
}

template <typename T, typename S> constexpr T align_up(T val, S alignment) {
  return align_down(val + alignment - 1, alignment);
}

template <class T> T *align_down(T *ptr, size_t alignment) {
  auto val = reinterpret_cast<uintptr_t>(ptr);
  return reinterpret_cast<T *>(align_down(val, alignment));
}

template <class T> T *align_up(T *ptr, size_t alignment) {
  auto val = reinterpret_cast<uintptr_t>(ptr);
  return reinterpret_cast<T *>(align_up(val, alignment));
}
}
