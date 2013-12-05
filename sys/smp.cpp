#include <algorithm>

#include <sys/cpu.hpp>
#include <sys/event_manager.hpp>
#include <sys/page_allocator.hpp>
#include <sys/smp.hpp>
#include <sys/tls.hpp>
#include <sys/vmem.hpp>

using namespace ebbrt;

extern char smpboot[];
extern char smpboot_end[];
extern char *smp_stack_free;

void ebbrt::smp_init() {
  tls_smp_init();
  char *stack_list = 0;
  auto num_aps = cpus.size() - 1;
  for (size_t i = 0; i < num_aps; i++) {
    auto pfn = page_allocator->Alloc();
    kbugon(pfn == 0, "Failed to allocate smp stack!\n");
    auto addr = pfn_to_addr(pfn);
    kbugon(addr >= 1 << 30, "Stack will not be accessible by APs!\n");
    *reinterpret_cast<char **>(addr) = stack_list;
    stack_list = reinterpret_cast<char *>(addr);
  }
  map_memory(pfn_down(SMP_START_ADDRESS), pfn_down(SMP_START_ADDRESS));
  smp_stack_free = stack_list;
  std::copy(smpboot, smpboot_end, reinterpret_cast<char *>(SMP_START_ADDRESS));
  // TODO: unmap memory

  for (size_t i = 1; i < cpus.size(); ++i) {
    auto apic_id = cpus[i].get_apic_id();
    apic_ipi(apic_id, 0, 1, DELIVERY_INIT);
    // FIXME: spin for a bit?
    apic_ipi(apic_id, SMP_START_ADDRESS >> 12, 1, DELIVERY_STARTUP);
  }
}

extern "C" __attribute__((noreturn)) void ebbrt::smp_main() {
  apic_init();
  auto id = apic_get_id();
  auto cpu_it =
      std::find_if(cpus.begin(), cpus.end(),
                   [=](const cpu & c) { return c.get_apic_id() == id; });
  kassert(cpu_it != cpus.end());
  size_t cpu_index = *cpu_it;
  kprintf("Core %llu online\n", cpu_index);
  vmem_ap_init(cpu_index);
  trans_ap_init(cpu_index);
  tls_ap_init(cpu_index);

  cpu_it->init();

  event_manager->StartLoop();
}
