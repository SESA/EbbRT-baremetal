#pragma once

#include <atomic>

namespace ebbrt {
class semaphore {
  std::atomic<uint32_t> count_;
public:
  explicit semaphore(uint32_t count) : count_{ count } {}

  void signal(uint32_t count = 1) {
    count_.fetch_add(count, std::memory_order_release);
  }

  void wait(uint32_t count = 1) {
    while (!trywait(count))
      ;
  }

  bool trywait(uint32_t count = 1) {
    auto val = count_.load(std::memory_order_relaxed);
    if (val >= count && count_.compare_exchange_weak(
                            val, val - count, std::memory_order_acquire,
                            std::memory_order_relaxed)) {
      return true;
    }
    return false;
  }
};
}
