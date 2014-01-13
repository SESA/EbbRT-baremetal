#include <sys/clock.hpp>
#include <sys/event_manager.hpp>
#include <sys/timer.hpp>

using namespace ebbrt;

const constexpr EbbId ApicTimer::static_id;

namespace {
const constexpr uint32_t MSR_X2APIC_LVT_TIMER = 0x832;
const constexpr uint32_t MSR_X2APIC_INIT_COUNT = 0x838;
const constexpr uint32_t MSR_X2APIC_CURRENT_COUNT = 0x839;
const constexpr uint32_t MSR_X2APIC_DCR = 0x83e;
}

ApicTimer::ApicTimer() {
  auto interrupt = event_manager->AllocateVector([this]() {
    auto now = clock_time();
    kassert(!timers_.empty());
    while (!timers_.empty() && timers_.begin()->first <= now) {
      //we assume timers are repeating so we add the next one
      auto& kv = *timers_.begin();
      auto it =
          timers_.emplace(std::piecewise_construct,
                          std::forward_as_tuple(now + std::get<1>(kv.second)),
                          std::move(kv.second));
      timers_.erase(timers_.begin());
      std::get<0>(it->second)();
      now = clock_time();
    }
    if (!timers_.empty()) {
      SetTimer(std::chrono::duration_cast<std::chrono::microseconds>(
          timers_.begin()->first - now));
    }
  });

  //Map timer to interrupt and enable one-shot mode
  wrmsr(MSR_X2APIC_LVT_TIMER, interrupt);
  wrmsr(MSR_X2APIC_DCR, 0x3);  // divide = 16

  //calibrate timer
  auto t = clock_time();
  //set timer
  wrmsr(MSR_X2APIC_INIT_COUNT, 0xFFFFFFFF);

  while (clock_time() < (t + std::chrono::milliseconds(10)))
    ;

  auto remaining = rdmsr(MSR_X2APIC_CURRENT_COUNT);

  //disable timer
  wrmsr(MSR_X2APIC_INIT_COUNT, 0);
  uint32_t elapsed = 0xFFFFFFFF;
  elapsed -= remaining;
  ticks_per_us_ = elapsed * 16 / 10000;
}

void ApicTimer::SetTimer(std::chrono::microseconds from_now) {
  uint64_t ticks = from_now.count();
  ticks *= ticks_per_us_;
  //determine timer divider
  auto divider = -1;
  while (ticks > 0xFFFFFFFF) {
    ticks >>= 1;
    divider++;
  }
  kbugon(divider > 7);  //max can divide by 128 (2^7)
  uint32_t divider_set;
  if (divider == -1) {
    divider_set = 0xb;
  } else {
    divider_set = divider;
  }
  wrmsr(MSR_X2APIC_DCR, divider_set);
  wrmsr(MSR_X2APIC_INIT_COUNT, ticks);
}

void ApicTimer::Start(std::chrono::microseconds timeout,
                      std::function<void()> f) {
  auto now = clock_time();
  auto when = now + timeout;
  if (timers_.empty() || timers_.begin()->first > when) {
    SetTimer(timeout);
  }

  timers_.emplace(std::piecewise_construct,
                  std::forward_as_tuple(when),
                  std::forward_as_tuple(std::move(f), timeout));
}
