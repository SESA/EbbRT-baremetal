#pragma once

#include <boost/intrusive/list.hpp>

#include <sys/cache_aligned.hpp>
#include <sys/cpu.hpp>
#include <sys/numa.hpp>
#include <sys/pfn.hpp>
#include <sys/spinlock.hpp>
#include <sys/trans.hpp>

namespace ebbrt {

class PageAllocator : public cache_aligned {
public:
  static const constexpr size_t MAX_ORDER = 11;
private:
  static_assert(sizeof(uint8_t) * 8 < MAX_ORDER,
                "page.order is not large enough to hold MAX_ORDER");
  spinlock lock_;
  nid_t nid_;

  static inline pfn_t pfn_to_buddy(pfn_t pfn, size_t order) {
    return pfn_t(((size_t)pfn) ^ (1 << order));
  }

  struct free_page {
    boost::intrusive::list_member_hook<> member_hook;

    pfn_t pfn() { return pfn_down(reinterpret_cast<uintptr_t>(this)); }
    pfn_t buddy(size_t order) { return pfn_to_buddy(pfn(), order); }
  };

  static inline free_page *pfn_to_free_page(pfn_t pfn) {
    auto addr = pfn_to_addr(pfn);
    return new (reinterpret_cast<void *>(addr)) free_page();
  }

  typedef boost::intrusive::list<
      free_page, boost::intrusive::member_hook<
                     free_page, boost::intrusive::list_member_hook<>,
                     &free_page::member_hook> > free_page_list;

  std::array<free_page_list, MAX_ORDER + 1> free_page_lists;
  friend void ebbrt::vmem_ap_init(size_t index);
  friend void ebbrt::trans_ap_init(size_t index);
  static boost::container::static_vector<PageAllocator, MAX_NUMA_NODES>
  allocators;

  static void early_free_page(pfn_t start, size_t order, nid_t nid);
  pfn_t AllocLocal(size_t order);
  void FreePageNoCoalesce(pfn_t pfn, size_t order);

public:
  static void Init();
  static PageAllocator& HandleFault(EbbId id);

  PageAllocator(nid_t nid);

  pfn_t Alloc(size_t order = 0, nid_t nid = my_node());
  void Free(pfn_t pfn, size_t order = 0);
};

constexpr auto page_allocator = EbbRef<PageAllocator>{ page_allocator_id };
}
