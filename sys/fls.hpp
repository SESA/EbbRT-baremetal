#pragma once

namespace ebbrt {
inline unsigned fls(uint64_t word) {
  return 63 - __builtin_clzll(word);
}
}
