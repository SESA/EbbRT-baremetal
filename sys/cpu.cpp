#include <sys/cpu.hpp>
#include <sys/page_allocator.hpp>

using namespace ebbrt;

thread_local cpu* ebbrt::my_cpu_tls;

boost::container::static_vector<cpu, MAX_NUM_CPUS> ebbrt::cpus;

char cpu::boot_interrupt_stack[PAGE_SIZE];

void cpu::init() {
  my_cpu_tls = this;
  gdt_.set_tss_addr(reinterpret_cast<uint64_t>(&atss_.tss));
  uint64_t interrupt_stack;
  if (index_ == 0) {
    interrupt_stack =
      reinterpret_cast<uint64_t>(boot_interrupt_stack + PAGE_SIZE);
  } else {
    auto page = page_allocator->Alloc();
    kbugon(page == 0, "Unable to allocate page for interrupt stack\n");
    interrupt_stack = pfn_to_addr(page) + PAGE_SIZE;
  }
  atss_.tss.set_ist_entry(1, interrupt_stack);
  gdt_.load();
  idt_load();
}

void cpu::set_event_stack(uintptr_t top_of_stack) {
  atss_.tss.set_ist_entry(2, top_of_stack);
}
