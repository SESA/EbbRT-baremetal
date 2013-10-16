#include <cstdarg>
#include <cstring>

#include <sys/acpi.hpp>
#include <sys/apic.hpp>
#include <sys/console.hpp>
#include <sys/cpu.hpp>
#include <sys/cpuid.hpp>
#include <sys/debug.hpp>
#include <sys/event.hpp>
#include <sys/gthread.hpp>
#include <sys/idt.hpp>
#include <sys/multiboot.hpp>
#include <sys/pci.hpp>
#include <sys/pmem.hpp>
#include <sys/power.hpp>
#include <sys/priority.hpp>
#include <sys/vmem.hpp>

namespace {
bool ebbrt_initialized = false;

__attribute__((constructor(EBBRT_PRIORITY))) void ebbrt_initialize_func() {
  ebbrt_initialized = true;
}

bool event_initialized = false;
__attribute__((constructor(EVENT_PRIORITY))) void
event_initialize_func() {
  event_initialized = true;
}

int last_index;
}

extern void (*start_ctors[])();
extern void (*end_ctors[])();

namespace ebbrt {
void kmain_cont() {
  for (int i = last_index + 1; i < (end_ctors - start_ctors); ++i) {
    start_ctors[i]();
    if (ebbrt_initialized) {
      break;
    }
  }
  acpi::init();
  pci::init();
  kprintf("Ebbrt\n");
}

extern "C" __attribute__((noreturn)) void kmain(MultibootInformation *mbi) {
  console::init();
  kprintf("EbbRT Copyright 2013 SESA Developers\n");
  cpuid::init();
  disable_legacy_pic();
  memcpy(&saved_mbi, mbi, sizeof(MultibootInformation));
  for (int i = 0; i < (end_ctors - start_ctors); ++i) {
    last_index = i;
    start_ctors[i]();
    if (event_initialized) {
      break;
    }
  }
  pmem::init();
  vmem::init();
  idt::init();
  cpu::init();
  gthread::init();
  event::init(kmain_cont);
}
}
