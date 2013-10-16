#pragma once

#include <cstdint>

namespace ebbrt {
class apic_t {
  uint32_t kvm_pv_eoi_word_;
public:
  apic_t();
  void self_ipi(uint8_t vector);
private:
  static const constexpr uint32_t SVR = 0x80f;
  static const constexpr uint32_t SELF_IPI = 0x83f;

  void software_enable();
};

  void disable_legacy_pic();
}
