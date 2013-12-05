#include <cstdint>
#include <cstring>

#include <sys/cpuid.hpp>

using namespace ebbrt;
struct result {
  uint32_t eax;
  uint32_t ebx;
  uint32_t ecx;
  uint32_t edx;
};

inline result cpuid(uint32_t leaf) {
  result r;
  asm("cpuid" : "=a"(r.eax), "=b"(r.ebx), "=c"(r.ecx), "=d"(r.edx) : "a"(leaf));
  return r;
}

cpuid_features_t ebbrt::features;

struct vendor_id_t {
  uint32_t ebx;
  uint32_t ecx;
  uint32_t edx;
};

vendor_id_t kvm_vendor_id = { 0x4b4d564b, 0x564b4d56, 0x4d };

struct cpuid_bit {
  uint32_t leaf;
  uint8_t reg;
  uint32_t bit;
  bool cpuid_features_t::*flag;
  vendor_id_t *vendor_id;
};

cpuid_bit cpuid_bits[] = { { 1, 2, 21, &cpuid_features_t::x2apic },
                           { 0x40000001,                    0,             6,
                             &cpuid_features_t::kvm_pv_eoi, &kvm_vendor_id }, };

constexpr size_t nr_cpuid_bits = sizeof(cpuid_bits) / sizeof(cpuid_bit);

void ebbrt::cpuid_init() {
  for (size_t i = 0; i < nr_cpuid_bits; ++i) {
    const auto &bit = cpuid_bits[i];
    auto vals = cpuid(bit.leaf & 0xf0000000);
    if (bit.vendor_id) {
      if (vals.ebx != bit.vendor_id->ebx || vals.ecx != bit.vendor_id->ecx ||
          vals.edx != bit.vendor_id->edx) {
        continue;
      }
    }
    if ((bit.leaf & 0xf0000000) == 0x40000000 &&
        bit.vendor_id == &kvm_vendor_id && vals.eax == 0) {
      vals.eax = 0x40000001; // kvm bug workaround
    }
    if (bit.leaf > vals.eax) {
      continue;
    }

    auto res = cpuid(bit.leaf);
    uint32_t res_array[4] = { res.eax, res.ebx, res.ecx, res.edx };
    uint32_t val = res_array[bit.reg];
    features.*(bit.flag) = (val >> bit.bit) & 1;
  }
}
