#include <cstring>

#include <sys/cpu.hpp>
#include <sys/tls.hpp>

struct thread_control_block {
  thread_control_block *self;
};

extern char tcb0[];
extern char tls_start[];
extern char tls_end[];

void ebbrt::tls_init() {
  auto tls_size = tls_end - tls_start;
  std::copy(tls_start, tls_end, tcb0);
  auto p = reinterpret_cast<thread_control_block *>(tcb0 + tls_size);
  p->self = p;

  wrmsr(IA32_FS_BASE, reinterpret_cast<uint64_t>(p));
}
