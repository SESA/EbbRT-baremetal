#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#include <boost/icl/interval_set.hpp>
#pragma GCC diagnostic pop

#include <sys/cache_aligned.hpp>
#include <sys/spinlock.hpp>
#include <sys/trans.hpp>

namespace ebbrt {
class EbbAllocator : public cache_aligned {
  spinlock lock_;
  boost::icl::interval_set<EbbId> free_ids_;

public:
  static void Init();
  static EbbAllocator &HandleFault(EbbId id);
  EbbAllocator();
  EbbId AllocateLocal();
};

constexpr auto ebb_allocator = EbbRef<EbbAllocator>{ ebb_allocator_id };
}
