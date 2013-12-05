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

class spin_barrier {
  const unsigned int n_;

  std::atomic<unsigned int> waiters_;

  std::atomic<unsigned int> completed_;

public:
  spin_barrier(unsigned int n) : n_(n), waiters_{ 0 }, completed_{ 0 } {}

  void wait() {
    auto step = completed_.load();

    if (waiters_.fetch_add(1) == n_ - 1) {
      waiters_.store(0);
      completed_.fetch_add(1);
    } else {
      while (completed_.load() == step)
        ;
    }
  }
};
}
