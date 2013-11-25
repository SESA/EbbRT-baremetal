#include <cinttypes>
#include <new>

#include <boost/utility.hpp>

#include <sys/align.hpp>
#include <sys/debug.hpp>
#include <sys/early_page_allocator.hpp>

using namespace ebbrt;

explicitly_constructed<free_pages_tree> ebbrt::free_pages;
void ebbrt::early_page_allocator_init() { free_pages.construct(); }

namespace {
typedef decltype(free_pages->begin()) free_page_iterator;
free_page_iterator coalesce(free_page_iterator a, free_page_iterator b) {
  if (a->end() != b->start())
    return b;

  a->npages += b->npages;
  free_pages->erase(b);
  return a;
}

void free_page_range(pfn_t start, pfn_t end, nid_t nid) {
  auto range = new (reinterpret_cast<void *>(pfn_to_addr(start)))
      page_range(end - start, nid);

  auto it = free_pages->insert(*range).first;
  if (it != free_pages->begin()) {
    it = coalesce(boost::prior(it), it);
  }
  if (boost::next(it) != free_pages->end()) {
    coalesce(it, boost::next(it));
  }
}
}

void ebbrt::early_free_memory_range(uint64_t begin, uint64_t length,
                                    nid_t nid) {
  auto pfn_start = pfn_up(begin);
  auto diff = pfn_to_addr(pfn_start) - begin;
  if (diff > length)
    return;

  auto pfn_end = pfn_down(begin + length - diff);
  if (pfn_end == pfn_start)
    return;

  free_page_range(pfn_start, pfn_end, nid);
}

pfn_t ebbrt::early_allocate_page(unsigned int npages, nid_t nid) {
  for (auto it = free_pages->begin(); it != free_pages->end(); ++it) {
    if (nid != ANY_NID && it->nid != nid)
      continue;

    auto ret = it->end() - npages;

    if (ret < it->start())
      continue;

    if (ret == it->start()) {
      free_pages->erase(it);
    } else {
      it->npages -= npages;
    }
    // the iterator is potentially no longer valid, don't use it

    return ret;
  }

  kprintf("%s: unabled to allocate %" PRIu64 " pages\n", __PRETTY_FUNCTION__,
          npages);
  kabort();
}

void ebbrt::early_free_page(pfn_t start, pfn_t end, nid_t nid) {
  free_page_range(start, end, nid);
}

void ebbrt::early_set_nid_range(pfn_t start, pfn_t end, nid_t nid) {
  for (auto it = free_pages->begin(); it != free_pages->end(); ++it) {
    if (it->end() <= start)
      continue;

    if (it->start() >= end)
      return;

    if (it->start() < start && it->end() > start) {
      // straddles below
      auto new_end = std::min(end, it->end());
      auto new_range = new (reinterpret_cast<void *>(pfn_to_addr(start)))
          page_range(new_end - start, nid);
      free_pages->insert(*new_range);
      it->npages = start - it->start();
      continue;
    }

    if (it->start() < end && it->end() > end) {
      // straddles above
      auto new_range = new (reinterpret_cast<void *>(pfn_to_addr(end)))
          page_range(it->end() - end, ANY_NID);
      free_pages->insert(*new_range);
      it->npages = end - it->start();
    }

    kprintf("Early Page Allocator NUMA mapping %#018" PRIx64 "-%#018" PRIx64
            " -> %u\n",
            pfn_to_addr(it->start()), pfn_to_addr(it->end()) - 1,
            (unsigned)nid);
    it->nid = nid;
  }
}
