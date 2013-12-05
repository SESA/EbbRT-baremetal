#include <sys/apic.hpp>
#include <sys/cpu.hpp>
#include <sys/cpuid.hpp>
#include <sys/debug.hpp>
#include <sys/io.hpp>

using namespace ebbrt;

void ebbrt::disable_pic() {
  if (!features.x2apic) {
    kprintf("No support for x2apic! Aborting\n");
    kabort();
  }

  // disable pic by masking all irqs
  out8(0x21, 0xff);
  out8(0xa1, 0xff);
}

const constexpr uint32_t MSR_X2APIC_IDR = 0x802;
const constexpr uint32_t MSR_X2APIC_SVR = 0x80f;
const constexpr uint32_t MSR_X2APIC_ICR = 0x830;

void ebbrt::apic_init() {
  auto apic_base = rdmsr(MSR_IA32_APIC_BASE);
  // enable x2apic mode
  wrmsr(MSR_IA32_APIC_BASE, apic_base | 0xc00);
  wrmsr(MSR_X2APIC_SVR, 0x100);

  // if (features.kvm_pv_eoi) {
  //   wrmsr(MSR_KVM_PV_EOI, reinterpret_cast<uint64_t>(&kvm_pv_eoi_word_));
  // }
}

void ebbrt::apic_ipi(uint8_t apic_id, uint8_t vector, bool level,
                     uint8_t delivery_mode) {
  auto val = (uint64_t(apic_id) << 32) | (uint64_t(level) << 14) |
             (uint64_t(delivery_mode) << 8) | vector;
  wrmsr(MSR_X2APIC_ICR, val);
}

uint32_t ebbrt::apic_get_id() {
  return rdmsr(MSR_X2APIC_IDR);
}
