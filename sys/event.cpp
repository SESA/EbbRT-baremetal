#include <sys/debug.hpp>
#include <sys/event.hpp>
#include <sys/pmem.hpp>

namespace ebbrt {
namespace event {
__attribute__((noreturn))
void event_loop(void (*func)()) {
  func();
  while (true)
    ;
}

void init(void (*func)()) {
  auto stack = pmem::allocate_page(1 << 21);

  asm volatile("mov %[func], %%rdi;"
               "mov %[stack], %%rsp;"
               "mov $0, %%rbp;"
               "jmp *%[event_loop]"
               :
               : [func] "r"(func), [stack] "r"(stack + (1 << 21)),
                 [event_loop] "r"(event_loop));
  kprintf("Init function returned!\n");
  kabort();
}
}
}
