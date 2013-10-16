#include <cinttypes>
#include <cstring>
#include <new>

#include <boost/intrusive/set.hpp>
#include <boost/utility.hpp>
#include <sys/align.hpp>
#include <sys/debug.hpp>
#include <sys/pmem.hpp>
#include <sys/multiboot.hpp>
#include <sys/priority.hpp>
#include <sys/spinlock.hpp>
#include <sys/vmem.hpp>

extern char kbegin[];
extern char kend[];

namespace ebbrt {
namespace pmem {

namespace bi = boost::intrusive;

struct page_range {
  inline explicit page_range(size_t size) noexcept : size{ size } {}
  size_t size;
  bi::set_member_hook<> size_member_hook;
  bi::set_member_hook<> addr_member_hook;

  uint64_t addr() const noexcept { return reinterpret_cast<uint64_t>(this); }
};

struct size_compare {
  bool operator()(const page_range &a, const page_range &b) const {
    return a.size < b.size;
  }
};

struct size_compare_key {
  bool operator()(const page_range &a, uint64_t size) const {
    return a.size < size;
  }
};

struct addr_compare {
  bool operator()(const page_range &a, const page_range &b) const {
    return a.addr() < b.addr();
  }
};

bi::set<page_range, bi::compare<size_compare>,
        bi::member_hook<page_range, bi::set_member_hook<>,
                        &page_range::size_member_hook> >
free_pages_by_size __attribute__((init_priority(FREEPAGE_INIT_PRIORITY)));

bi::set<page_range, bi::compare<addr_compare>,
        bi::member_hook<page_range, bi::set_member_hook<>,
                        &page_range::addr_member_hook> >
free_pages_by_addr __attribute__((init_priority(FREEPAGE_INIT_PRIORITY)));

spinlock free_pages_lock __attribute__((init_priority(FREEPAGE_INIT_PRIORITY)));

void free_page_range_locked(page_range *range) {
  free_pages_by_addr.insert(*range);
  free_pages_by_size.insert(*range);
}

void free_page_range_locked(uintptr_t begin, uintptr_t end) {
  auto range = new (reinterpret_cast<void *>(begin)) page_range(end - begin);
  free_page_range_locked(range);
}

void free_page_range(uintptr_t begin, uintptr_t end) {
  std::lock_guard<spinlock> lock(free_pages_lock);
  free_page_range_locked(begin, end);
}

void free_memory_range(uintptr_t begin, uintptr_t end) {
  auto begin_aligned = align_up(begin, page_size);
  auto end_aligned = align_down(end, page_size);
  if (begin_aligned >= end_aligned) {
    return;
  }

  free_page_range(begin_aligned, end_aligned);
}

uint64_t allocate_page(uint64_t size) {
  std::lock_guard<spinlock> lock(free_pages_lock);
  for (auto it = free_pages_by_size.lower_bound(size, size_compare_key());
       it != free_pages_by_size.end(); ++it) {
    auto begin_addr = it->addr();
    // return the last aligned chunk of the requested size
    auto ret_addr = (begin_addr + it->size - size) & ~(size - 1);
    if (ret_addr < begin_addr) {
      continue;
    }

    auto end_size = begin_addr + it->size - ret_addr - size;

    if (ret_addr == begin_addr) {
      free_pages_by_addr.erase(free_pages_by_addr.iterator_to(*it));
      free_pages_by_size.erase(it);
    } else {
      it->size = ret_addr - begin_addr;
      auto &fp = *it;
      free_pages_by_size.erase(it);
      free_pages_by_size.insert(fp);
    }
    // the iterator is potentially no longer valid, don't use it

    if (end_size > 0) {
      free_page_range_locked(ret_addr + size, ret_addr + size + end_size);
    }

    return ret_addr;
  }
  // TODO: coalesce
  kprintf("%s: unable to allocate %" PRIu64 " bytes\n", __PRETTY_FUNCTION__, size);
  kabort();
}

void init() {
  // copy memory map to the stack so we don't lose it
  char memmap_buffer[saved_mbi.memory_map_length];
  uint32_t memmap_length = saved_mbi.memory_map_length;
  memcpy(memmap_buffer, reinterpret_cast<void *>(saved_mbi.memory_map_address),
         memmap_length);

  constexpr uintptr_t initial_map_end = 1 << 30; // 1GB initial map
  MultibootMemoryRegion::for_each_mmregion(memmap_buffer, memmap_length,
                                           [](MultibootMemoryRegion mmregion) {

    auto kend_addr = reinterpret_cast<uintptr_t>(kend);
    if (mmregion.end() <= kend_addr) {
      return;
    }

    uint64_t begin = mmregion.begin();
    uint64_t length = mmregion.length();

    mmregion.trim_below(kend_addr);

    mmregion.trim_above(initial_map_end);
    if (mmregion.begin() < initial_map_end) {
      free_memory_range(mmregion.begin(), mmregion.end());
    }
    vmem::map(begin, begin, length);
  });
  vmem::enable_runtime_page_table();
  MultibootMemoryRegion::for_each_mmregion(memmap_buffer, memmap_length,
                                           [](MultibootMemoryRegion mmregion) {
    if (mmregion.end() < initial_map_end) {
      return;
    }
    mmregion.trim_below(initial_map_end);
    free_memory_range(mmregion.begin(), mmregion.end());
  });
}
}
}
