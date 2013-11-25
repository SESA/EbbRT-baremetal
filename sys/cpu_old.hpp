#pragma once
#include <cstdint>

namespace ebbrt {
constexpr uint32_t MSR_IA32_APIC_BASE = 0x0000001b;
constexpr uint32_t MSR_KVM_PV_EOI = 0x4b564d04;

inline uint64_t rdmsr(uint32_t index) {
  uint32_t low, high;
  asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(index));
  return low | (uint64_t(high) << 32);
}

inline void wrmsr(uint32_t index, uint64_t data) {
  uint32_t low = data;
  uint32_t high = data >> 32;
  asm volatile("wrmsr" : : "c"(index), "a"(low), "d"(high));
}

namespace cpu {
  void init();
}
}
