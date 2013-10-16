#pragma once

#include <cstdarg>
#include <cstdio>

namespace ebbrt {
void kvprintf(const char *__restrict format, va_list va);
void kprintf(const char *__restrict format, ...);

__attribute__((noreturn)) void kabort();

#define UNIMPLEMENTED()                                                        \
  do {                                                                         \
    ebbrt::kprintf("%s: unimplemented\n", __PRETTY_FUNCTION__);                \
    ebbrt::kabort();                                                           \
  } while (0)
}
