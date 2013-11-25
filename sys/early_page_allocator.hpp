#pragma once

#include <boost/intrusive/set.hpp>

#include <sys/explicitly_constructed.hpp>
#include <sys/numa.hpp>
#include <sys/pfn.hpp>

namespace ebbrt {
void early_page_allocator_init();
void early_free_memory_range(uint64_t begin, uint64_t length,
                             nid_t nid = ANY_NID);
pfn_t early_allocate_page(unsigned int npages = 1, nid_t nid = ANY_NID);
void early_free_page(pfn_t start, pfn_t end, nid_t nid = ANY_NID);
void early_set_nid_range(pfn_t start, pfn_t end, nid_t nid);

struct page_range {
  inline explicit page_range(size_t npages, nid_t nid) noexcept
      : npages{ npages },
        nid{ nid } {}
  size_t npages;
  nid_t nid;
  boost::intrusive::set_member_hook<> member_hook;

  pfn_t start() const noexcept {
    return pfn_down(reinterpret_cast<uintptr_t>(this));
  }

  pfn_t end() const noexcept { return start() + npages; }
};

inline bool operator<(const page_range &lhs, const page_range &rhs) noexcept {
  return lhs.start() < rhs.start();
}

typedef boost::intrusive::set<
    page_range, boost::intrusive::member_hook<
                    page_range, boost::intrusive::set_member_hook<>,
                    &page_range::member_hook> > free_pages_tree;
extern explicitly_constructed<free_pages_tree> free_pages;

template <typename F> void release_free_pages(F f) {
  auto it = free_pages->begin();
  auto end = free_pages->end();
  while (it != end) {
    auto start = it->start();
    auto end = it->end();
    auto nid = it->nid;
    //CAREFUL: advance the iterator before passing the free page
    ++it;
    f(start, end, nid);
  }
  free_pages->clear();
}
}
