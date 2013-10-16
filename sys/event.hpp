#pragma once

#include <cstdint>

namespace ebbrt {
namespace event {
inline uint32_t get_event_id() {
  return 0;
}
__attribute__((noreturn))
void init(void (*func)());
}
}
