#include <sys/apic.hpp>
#include <sys/cpu.hpp>
#include <sys/cpuid.hpp>
#include <sys/debug.hpp>
#include <sys/io.hpp>
#include <sys/priority.hpp>

namespace ebbrt {
void disable_legacy_pic() {
  if (!cpuid::features.x2apic) {
    kprintf("No support for x2apic! Aborting\n");
    kabort();
  }

  // disable pic by masking all irqs
  out8(0x21, 0xff);
  out8(0xa1, 0xff);
}

apic_t::apic_t() {
  auto apic_base = rdmsr(MSR_IA32_APIC_BASE);
  // enable x2apic mode
  wrmsr(MSR_IA32_APIC_BASE, apic_base | 0xc00);
  software_enable();

  if (cpuid::features.kvm_pv_eoi) {
    wrmsr(MSR_KVM_PV_EOI, reinterpret_cast<uint64_t>(&kvm_pv_eoi_word_));
  }
}

void apic_t::software_enable() { wrmsr(SVR, 0x100); }
void apic_t::self_ipi(uint8_t vector) { wrmsr(SELF_IPI, vector); }
}
