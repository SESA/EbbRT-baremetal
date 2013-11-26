#include <boost/container/static_vector.hpp>

#include <sys/align.hpp>
#include <sys/cpu.hpp>
#include <sys/debug.hpp>
#include <sys/early_page_allocator.hpp>
#include <sys/mem_map.hpp>
#include <sys/page_allocator.hpp>

using namespace ebbrt;

boost::container::static_vector<PageAllocator, MAX_NUMA_NODES>
PageAllocator::allocators;

void PageAllocator::Init() {
  for (unsigned i = 0; i < numa_nodes.size(); ++i) {
    allocators.emplace_back((nid_t)i);
  }

  release_free_pages([](pfn_t start, pfn_t end, nid_t nid) {
    kassert(end - start != 0);
    // compute largest power of two that fits in the range
    auto largest_pow2 =
        1 << (sizeof(unsigned int) * 8 - __builtin_clz(end - start) - 1);
    // find the divide at which point the pages decrease in size
    auto divide = align_up((uintptr_t)start, largest_pow2);
    auto pfn = start;
    // Add all the pages up to the divide
    while (pfn != divide) {
      kassert(pfn < divide);
      size_t order = __builtin_ctz(pfn);
      order = order > MAX_ORDER ? MAX_ORDER : order;
      early_free_page(pfn, order, nid);
      pfn += 1 << order;
    }
    // now add the rest
    pfn = divide;
    while (pfn != end) {
      kassert(pfn < end);
      auto order = sizeof(unsigned int) * 8 - __builtin_clz(end - pfn) - 1;
      order = order > MAX_ORDER ? MAX_ORDER : order;
      early_free_page(pfn, order, nid);
      pfn += 1 << order;
    }
  });
}

void PageAllocator::early_free_page(pfn_t start, size_t order,
                                           nid_t nid) {
  kassert(order <= MAX_ORDER);
  auto entry = pfn_to_free_page(start);
  allocators[nid].free_page_lists[order].push_front(*entry);
  auto page = pfn_to_page(start);
  kassert(page != nullptr);
  page->usage = page::Usage::PAGE_ALLOCATOR;
  page->data.order = order;
}

PageAllocator& PageAllocator::HandleFault(EbbId id) {
  kassert(id == page_allocator_id);
  auto& allocator = allocators[my_node()];
  cache_ref(id, allocator);
  return allocator;
}

PageAllocator::PageAllocator(nid_t nid) : nid_(nid) {}

pfn_t PageAllocator::AllocLocal(size_t order) {
  std::lock_guard<spinlock> lock(lock_);

  free_page *fp = nullptr;
  auto this_order = order;
  while (this_order <= MAX_ORDER) {
    auto &list = free_page_lists[this_order];
    if (!list.empty()) {
      fp = &list.front();
      list.pop_front();
      break;
    }
    ++this_order;
  }
  if (fp == nullptr) {
    return NO_PFN;
  }

  while (this_order != order) {
    --this_order;
    FreePageNoCoalesce(fp->buddy(this_order), this_order);
  }

  auto pfn = fp->pfn();
  auto page = pfn_to_page(fp->pfn());
  kassert(page != nullptr);
  page->usage = page::Usage::IN_USE;
  return pfn;
}

pfn_t PageAllocator::Alloc(size_t order, nid_t nid) {
  if (nid == nid_) {
    return AllocLocal(order);
  } else {
    return allocators[(size_t)nid].AllocLocal(order);
  }
}

void PageAllocator::FreePageNoCoalesce(pfn_t pfn, size_t order) {
  auto entry = pfn_to_free_page(pfn);
  free_page_lists[order].push_front(*entry);
  auto page = pfn_to_page(pfn);
  kassert(page != nullptr);
  page->usage = page::Usage::PAGE_ALLOCATOR;
  page->data.order = order;
}

void PageAllocator::Free(pfn_t pfn, size_t order) {
  std::lock_guard<spinlock> lock(lock_);
  kassert(order <= MAX_ORDER);
  while (order < MAX_ORDER) {
    auto buddy = pfn_to_buddy(pfn, order);
    auto page = pfn_to_page(buddy);
    if (page == nullptr || page->usage != page::Usage::PAGE_ALLOCATOR ||
        page->data.order != order) {
      break;
    }
    // Buddy is free, remove it from the buddy system
    auto entry = reinterpret_cast<free_page *>(pfn_to_addr(buddy));
    auto it = free_page_lists[order].iterator_to(*entry);
    free_page_lists[order].erase(it);
    order++;
  }
  FreePageNoCoalesce(pfn, order);
}
