#include <atomic>

#include <sys/clock.hpp>
#include <sys/cpu.hpp>
#include <sys/cpuid.hpp>
#include <sys/debug.hpp>

using namespace ebbrt;

namespace {
struct pvclock_wall_clock {
  std::atomic<volatile uint32_t> version;
  std::atomic<volatile uint32_t> sec;
  std::atomic<volatile uint32_t> nsec;
};

static_assert(sizeof(pvclock_wall_clock) == 12,
              "pvclock_wall_clock packing issue");

pvclock_wall_clock wall_clock;

struct pvclock_vcpu_time_info {
  std::atomic<volatile uint32_t> version;
  std::atomic<volatile uint32_t> pad0;
  std::atomic<volatile uint64_t> tsc_timestamp;
  std::atomic<volatile uint64_t> system_time;
  std::atomic<volatile uint32_t> tsc_to_system_mul;
  std::atomic<volatile int8_t> tsc_shift;
  std::atomic<volatile uint8_t> flags;
  std::atomic<volatile uint8_t> pad[2];
};

static_assert(sizeof(pvclock_vcpu_time_info) == 32,
              "pvclock_vcpu_time_info packing issue");

thread_local pvclock_vcpu_time_info vcpu_time_info;
std::chrono::nanoseconds boot_time;

const constexpr uint32_t MSR_KVM_WALL_CLOCK_NEW = 0x4b564d00;
const constexpr uint32_t MSR_KVM_SYSTEM_TIME_NEW = 0x4b564d01;

void setup_system_time() {
  wrmsr(MSR_KVM_SYSTEM_TIME_NEW,
        reinterpret_cast<uint64_t>(&vcpu_time_info) | 1);
}
}

void ebbrt::clock_init() {
  kbugon(!features.kvm_clocksource2, "No KVM clock!\n");

  wrmsr(MSR_KVM_WALL_CLOCK_NEW, reinterpret_cast<uint64_t>(&wall_clock));

  uint32_t version, sec, nsec;
  do {
    //if the version value is odd, then an update is in progress, retry
    if ((version = wall_clock.version.load(std::memory_order_relaxed)) % 2)
      continue;
    std::atomic_thread_fence(std::memory_order_acquire);
    sec = wall_clock.sec.load(std::memory_order_relaxed);
    nsec = wall_clock.nsec.load(std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_acquire);
  } while (version != wall_clock.version.load(std::memory_order_relaxed));
  boot_time = std::chrono::seconds(sec) + std::chrono::nanoseconds(nsec);
  setup_system_time();
}

void ebbrt::clock_ap_init() { setup_system_time(); }

std::chrono::nanoseconds ebbrt::clock_time() {
  uint32_t version;
  uint64_t tsc = 0;
  uint64_t system_time = 0;
  uint32_t tsc_to_system_mul = 0;
  int8_t tsc_shift = 0;
  do {
    if ((version = vcpu_time_info.version.load(std::memory_order_relaxed)) % 2)
      continue;

    std::atomic_thread_fence(std::memory_order_acquire);
    tsc = vcpu_time_info.tsc_timestamp.load(std::memory_order_relaxed);
    system_time = vcpu_time_info.system_time.load(std::memory_order_relaxed);
    tsc_to_system_mul =
        vcpu_time_info.tsc_to_system_mul.load(std::memory_order_relaxed);
    tsc_shift = vcpu_time_info.tsc_shift.load(std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_acquire);
  } while (version != vcpu_time_info.version.load(std::memory_order_relaxed));

  auto time = rdtsc() - tsc;
  if (tsc_shift >= 0)
    time <<= tsc_shift;
  else
    time >>= -tsc_shift;
  uint64_t tmp;
  asm ("mulq %[multiplier];"
       "shrd $32, %[hi], %[lo]"
       : [lo] "+a" (time),
         [hi] "=d" (tmp)
       : [multiplier] "rm" (static_cast<uint64_t>(tsc_to_system_mul)));
  return boot_time + std::chrono::nanoseconds(system_time + time);
}
