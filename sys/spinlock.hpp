#pragma once

#include <atomic>
#include <mutex>

namespace ebbrt {
class spinlock {
  std::atomic_flag lock_;

public:
  spinlock() : lock_{ ATOMIC_FLAG_INIT } {}
  void lock() {
    while (lock_.test_and_set(std::memory_order_acquire))
      ;
  }
  void unlock() { lock_.clear(std::memory_order_release); }
};
}
