#pragma once

#include <atomic>
#include <mutex>
#include <sys/event.hpp>

namespace ebbrt {
  class recursive_spinlock {
    uint32_t event_id_;
    uint16_t count_;
    spinlock lock_;
  public:
    recursive_spinlock() : count_{0} {}
    void lock() {
      while (!trylock())
        ;
    }
    bool trylock() {
      std::lock_guard<spinlock> lock{lock_};
      if (count_ == UINT16_MAX) {
        return false;
      }
      if (count_ <= 0) {
        event_id_ = event::get_event_id();
        count_++;
        return true;
      }

      if (event_id_ == event::get_event_id()) {
        count_++;
        return true;
      }
      return false;
    }
    void unlock() {
      std::lock_guard<spinlock> lock{lock_};
      count_--;
    }
  };
}
