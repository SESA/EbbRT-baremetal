#pragma once

#include <chrono>
#include <map>

#include <sys/multicore_ebb_static.hpp>
#include <sys/trans.hpp>

namespace ebbrt {

class ApicTimer : public multicore_ebb_static<ApicTimer> {
  uint64_t ticks_per_us_;
  std::multimap<std::chrono::nanoseconds,
                std::tuple<std::function<void()>, std::chrono::microseconds> >
      timers_;

  void SetTimer(std::chrono::microseconds from_now);

 public:
  static const constexpr EbbId static_id = apic_timer_id;
  ApicTimer();
  void Start(std::chrono::microseconds timeout, std::function<void()> f);
};

const constexpr auto timer = EbbRef<ApicTimer>(ApicTimer::static_id);
}
