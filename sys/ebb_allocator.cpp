#include <sys/debug.hpp>
#include <sys/ebb_allocator.hpp>
#include <sys/explicitly_constructed.hpp>

using namespace ebbrt;

namespace {
explicitly_constructed<EbbAllocator> the_allocator;
}

void EbbAllocator::Init() { the_allocator.construct(); }

EbbAllocator &EbbAllocator::HandleFault(EbbId id) {
  kassert(id == ebb_allocator_id);
  auto &ref = *the_allocator;
  cache_ref(id, ref);
  return ref;
}

EbbAllocator::EbbAllocator() {
  auto start_ids =
      boost::icl::interval<EbbId>::type{ FIRST_FREE_ID, (1 << 16) - 1 };
  free_ids_.insert(std::move(start_ids));
}

EbbId EbbAllocator::AllocateLocal() {
  std::lock_guard<spinlock> lock{ lock_ };
  kbugon(free_ids_.empty());
  auto ret = boost::icl::first(free_ids_);
  free_ids_.erase(ret);
  return ret;
}
