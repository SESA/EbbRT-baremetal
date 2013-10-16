#pragma once

namespace ebbrt {
namespace cpuid {
struct features_t {
  bool x2apic;
  bool kvm_pv_eoi;
};

extern features_t features;

void init();
}
}
