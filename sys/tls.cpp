#include <cstring>

#include <boost/container/static_vector.hpp>

#include <sys/cpu.hpp>
#include <sys/general_purpose_allocator.hpp>
#include <sys/tls.hpp>

using namespace ebbrt;

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

  wrmsr(MSR_IA32_FS_BASE, reinterpret_cast<uint64_t>(p));
}

namespace {
boost::container::static_vector<void *, MAX_NUM_CPUS> tls_ptrs;
}

void ebbrt::tls_smp_init() {
  tls_ptrs.emplace_back(static_cast<void *>(tcb0));
  auto tls_size = align_up(tls_end - tls_start + 8, 64);
  for (size_t i = 1; i < cpus.size(); ++i) {
    auto nid = cpus[i].get_nid();
    auto ptr = gp_allocator->Alloc(tls_size, nid);
    kbugon(ptr == nullptr, "Failed to allocate TLS region\n");
    tls_ptrs.emplace_back(ptr);
  }
}

void ebbrt::tls_ap_init(size_t index) {
  auto tcb = static_cast<char *>(tls_ptrs[index]);
  std::copy(tls_start, tls_end, tcb);
  auto tls_size = tls_end - tls_start;
  auto p = reinterpret_cast<thread_control_block *>(tcb + tls_size);
  p->self = p;

  wrmsr(MSR_IA32_FS_BASE, reinterpret_cast<uint64_t>(p));
}
