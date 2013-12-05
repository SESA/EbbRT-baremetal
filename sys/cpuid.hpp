#pragma once

namespace ebbrt {
struct cpuid_features_t {
  bool x2apic;
  bool kvm_pv_eoi;
};

extern cpuid_features_t features;

void cpuid_init();
}
