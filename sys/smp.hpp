#pragma once

#define SMP_START_ADDRESS (0x4000)

#ifndef ASSEMBLY
namespace ebbrt {
  void smp_init();
  extern "C" __attribute__((noreturn)) void smp_main();
}
#endif
