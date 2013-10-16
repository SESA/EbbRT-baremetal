#include <algorithm>
#include <cinttypes>
#include <cstdint>
#include <cstring>

#include <boost/utility.hpp>
#include <boost/intrusive/set.hpp>
#include <sys/debug.hpp>
#include <sys/pmem.hpp>
#include <sys/priority.hpp>
#include <sys/spinlock.hpp>
#include <sys/vmem.hpp>

namespace ebbrt {
namespace vmem {

class pte {
  uint64_t raw_;

public:
  bool present() const { return raw_ & 1; }
  uint64_t addr(bool large) const {
    auto ret = raw_ & ((UINT64_C(1) << 52) - 1);
    ret &= ~((UINT64_C(1) << 12) - 1);
    ret &= ~(static_cast<uint64_t>(large) << 12); // ignore PAT bit
    return ret;
  }
  bool large() const { return raw_ & (1 << 7); }
  void clear() { raw_ = 0; }
  void set_present(bool val) { set_bit(0, val); }
  void set_writable(bool val) { set_bit(1, val); }
  void set_large(bool val) { set_bit(7, val); }
  void set_nx(bool val) { set_bit(63, val); }
  void set_addr(uint64_t addr, bool large) {
    auto mask = (0x8000000000000fff | static_cast<uint64_t>(large) << 12);
    raw_ = (raw_ & mask) | addr;
  }
  static constexpr uint32_t perm_read = 1 << 0;
  static constexpr uint32_t perm_write = 1 << 1;
  static constexpr uint32_t perm_exec = 1 << 2;
  void set(uint64_t phys_addr, bool large,
           unsigned permissions = perm_read | perm_write | perm_exec) {
    raw_ = 0;
    set_present(permissions != 0);
    set_writable(permissions & perm_write);
    set_large(large);
    set_addr(phys_addr, large);
    set_nx(!(permissions & perm_exec));
  }
  void set_normal(uint64_t phys_addr,
                  uint32_t permissions = perm_read | perm_write | perm_exec) {
    set(phys_addr, false, permissions);
  }
  void set_large(uint64_t phys_addr,
                 uint32_t permissions = perm_read | perm_write | perm_exec) {
    set(phys_addr, true, permissions);
  }

private:
  void set_bit(size_t bit_num, bool value) {
    raw_ &= ~(UINT64_C(1) << bit_num);
    raw_ |= static_cast<uint64_t>(value) << bit_num;
  }
};

inline uint64_t canonical(uint64_t addr) {
  // sign extend for x86 canonical address
  return (int64_t(addr) << 16) >> 16;
}

pte page_table_root;

size_t pt_index(uintptr_t virt_addr, size_t level) {
  return (virt_addr >> (12 + level * 9)) & ((1 << 9) - 1);
}

constexpr size_t num_page_sizes = 2;

void map_level(pte &entry, uintptr_t virt_start, uintptr_t virt_end,
               uint64_t phys_start, uintptr_t base_virt, size_t level) {
  if (!entry.present()) {
    // allocate the entry
    auto page = pmem::allocate_page();
    pte *pt = reinterpret_cast<pte *>(page);
    for (int i = 0; i < 512; ++i) {
      pt[i].clear();
    }

    entry.set_normal(page);
  }

  --level;
  auto pt = reinterpret_cast<pte *>(entry.addr(false));
  uint64_t step = UINT64_C(1) << (12 + level * 9);
  auto idx_begin = pt_index(std::max(virt_start, base_virt), level);
  auto base_virt_end = base_virt + step * 512 - 1;
  base_virt_end = canonical(base_virt_end);
  auto idx_end = pt_index(std::min(virt_end - 1, base_virt_end), level);
  base_virt += idx_begin * step;
  // sign extend for x86 canonical address
  base_virt = canonical(base_virt);
  for (size_t idx = idx_begin; idx <= idx_end; ++idx) {
    if (level < num_page_sizes && virt_start <= base_virt &&
        virt_end >= base_virt + step) {
      pt[idx].set(phys_start + (base_virt - virt_start), level > 0);
      asm volatile("invlpg (%[addr])" : : [addr] "r"(base_virt) : "memory");
    } else {
      map_level(pt[idx], virt_start, virt_end, phys_start, base_virt, level);
    }
    base_virt += step;
    // sign extend for x86 canonical address
    base_virt = canonical(base_virt);
  }
}

void map(uint64_t virt_addr, uint64_t phys_addr, uint64_t size) {
  map_level(page_table_root, virt_addr, virt_addr + size, phys_addr, 0, 4);
}

void enable_runtime_page_table() {
  asm volatile("mov %[page_table], %%cr3"
               :
               : [page_table] "r"(page_table_root));
}

namespace bi = boost::intrusive;

struct page_range {
  inline explicit page_range(uint64_t addr_, size_t size_)
      : addr{ addr_ }, size{ size_ } {}
  uint64_t addr;
  size_t size;
  bi::set_member_hook<> member_hook;
};

bool operator<(const page_range &a, const page_range &b) {
  return a.addr < b.addr;
}

struct delete_disposer {
  void operator()(page_range *delete_this) { delete delete_this; }
};

bi::set<page_range, bi::member_hook<page_range, bi::set_member_hook<>,
                                    &page_range::member_hook> >
free_pages __attribute__((init_priority(VIRT_FREEPAGE_INIT_PRIORITY)));

spinlock free_pages_lock
    __attribute__((init_priority(VIRT_FREEPAGE_INIT_PRIORITY)));

typedef decltype(free_pages)::iterator free_page_iterator;

free_page_iterator coalesce(const free_page_iterator &a,
                            const free_page_iterator &b) {
  if (a->addr + a->size == b->addr) {
    a->size += b->size;
    free_pages.erase_and_dispose(b, delete_disposer());
    return a;
  } else {
    return b;
  }
}

void free_page_range_locked(uint64_t begin, uint64_t len) {
  auto range = new page_range(begin, len);
  auto it = free_pages.insert(*range).first;
  if (it != free_pages.begin()) {
    // coalesce before
    it = coalesce(boost::prior(it), it);
  }
  if (boost::next(it) != free_pages.end()) {
    // coalesce after
    coalesce(it, boost::next(it));
  }
}

void free_page_range(uint64_t begin, uint64_t len) {
  std::lock_guard<spinlock> lock(free_pages_lock);
  free_page_range_locked(begin, len);
}

void virt_free_level(pte &entry, uintptr_t base_virt, size_t level) {
  --level;
  auto pt = reinterpret_cast<pte *>(entry.addr(false));
  uint64_t step = UINT64_C(1) << (12 + level * 9);
  for (size_t idx = 0; idx < 512; idx++) {
    auto bvirt = base_virt + step * idx;
    bvirt = (int64_t(bvirt) << 16) >> 16;
    if (bvirt + step <= (1 << 20) || bvirt >= 0xffff800000000000) {
      continue;
    }
    if (!pt[idx].present()) {
      free_page_range(bvirt, step);
    } else if (level >= num_page_sizes || (level > 0 && !pt[idx].large())) {
      virt_free_level(pt[idx], bvirt, level);
    }
  }
}

void init() { virt_free_level(page_table_root, 0, 4); }

uint64_t allocate_page(uint64_t size) {
  std::lock_guard<spinlock> lock(free_pages_lock);
  for (auto it = free_pages.begin(); it != free_pages.end(); ++it) {
    if (it->size < size) {
      continue;
    }

    // return the last aligned chunk of the requested size
    auto ret_addr = (it->addr + it->size - size) & ~(pmem::page_size - 1);
    if (ret_addr < it->addr) {
      continue;
    }

    auto end_size = it->addr + it->size - ret_addr - size;

    if (ret_addr == it->addr) {
      free_pages.erase_and_dispose(it, delete_disposer());
    } else {
      it->size = ret_addr - it->addr;
    }
    // the iterator is potentially no longer valid, don't use it

    if (end_size > 0) {
      free_page_range_locked(ret_addr + size, end_size);
    }

    return ret_addr;
  }
  kprintf("%s: unable to allocate %" PRIu64 " bytes\n", __PRETTY_FUNCTION__,
          size);
  kabort();
}
}
}
