#pragma once

namespace ebbrt {
struct cpuid_features_t {
  bool x2apic;
  bool kvm_pv_eoi;
  bool kvm_clocksource2;
};

extern cpuid_features_t features;

void cpuid_init();
}
