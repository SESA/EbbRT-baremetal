#include <cstdarg>
#include <cstring>

#include <sys/acpi.hpp>
// #include <sys/apic.hpp>
#include <sys/console.hpp>
// #include <sys/cpu.hpp>
// #include <sys/cpuid.hpp>
#include <sys/debug.hpp>
#include <sys/e820.hpp>
#include <sys/early_page_allocator.hpp>
#include <sys/ebb_allocator.hpp>
// #include <sys/event.hpp>
#include <sys/general_purpose_allocator.hpp>
// #include <sys/gthread.hpp>
// #include <sys/idt.hpp>
#include <sys/local_id_map.hpp>
#include <sys/mem_map.hpp>
#include <sys/multiboot.hpp>
#include <sys/numa.hpp>
#include <sys/page_allocator.hpp>
// #include <sys/pci.hpp>
//#include <sys/pmem.hpp>
// #include <sys/power.hpp>
// #include <sys/priority.hpp>
#include <sys/slab_allocator.hpp>
#include <sys/tls.hpp>
#include <sys/trans.hpp>
#include <sys/vmem.hpp>

using namespace ebbrt;

namespace {
bool started_once = false;
}

extern "C" __attribute__((noreturn)) void kmain(MultibootInformation *mbi) {
  console::init();

  /* If by chance we reboot back into the kernel, panic */
  kbugon(started_once, "EbbRT reboot detected... aborting!\n");
  started_once = true;

  kprintf("EbbRT Copyright 2013 SESA Developers\n");

  idt_init();
  //bring up the first cpu structure early
  cpus.emplace_back(0, 0, 0);
  cpus[0].init();
  // cpuid::init();
  // disable_legacy_pic();

  tls_init();
  e820_init(mbi);
  e820_print_map();
  early_page_allocator_init();
  extern char kend[];
  auto kend_addr = reinterpret_cast<uint64_t>(kend);
  const constexpr uint64_t initial_map_end = 1 << 30;
  e820_for_each_usable_region([=](e820_entry entry) {
    if (entry.addr + entry.length <= kend_addr)
      return;

    auto begin = entry.addr;
    auto length = entry.length;

    entry.trim_below(kend_addr);
    entry.trim_above(initial_map_end);
    if (entry.addr < initial_map_end) {
      early_free_memory_range(entry.addr, entry.length);
    }
    early_map_memory(begin, length);
  });

  enable_runtime_page_table();

  e820_for_each_usable_region([=](e820_entry entry) {
    if (entry.addr + entry.length < initial_map_end)
      return;

    entry.trim_below(initial_map_end);
    early_free_memory_range(entry.addr, entry.length);
  });

  acpi_boot_init();
  numa_init();
  mem_map_init();
  trans_init();
  PageAllocator::Init();
  slab_init();
  gp_type::Init();
  LocalIdMap::Init();
  EbbAllocator::Init();

  kprintf("Finished\n");
  while (true)
    ;
}
