#pragma once

#include <array>
#include <cstdint>
#include <type_traits>

#include <boost/intrusive/list.hpp>

#include <sys/debug.hpp>
#include <sys/explicitly_constructed.hpp>
#include <sys/numa.hpp>
#include <sys/pfn.hpp>
#include <sys/slab_object.hpp>

namespace ebbrt {
const constexpr size_t MAX_PHYSMEM_BITS = 46;
const constexpr size_t SECTION_SIZE_BITS = 27;
const constexpr size_t PFN_SECTION_SHIFT = SECTION_SIZE_BITS - PAGE_SHIFT;
const constexpr size_t PAGES_PER_SECTION = 1 << PFN_SECTION_SHIFT;
const constexpr size_t PAGE_SECTION_MASK = (~(PAGES_PER_SECTION - 1));
const constexpr size_t SECTIONS_SHIFT = MAX_PHYSMEM_BITS - SECTION_SIZE_BITS;
const constexpr size_t NUM_MEM_SECTIONS = 1 << SECTIONS_SHIFT;

struct SlabCache;

struct page {
  enum class Usage : uint8_t {
    RESERVED,
    PAGE_ALLOCATOR,
    SLAB_ALLOCATOR,
    IN_USE
  } usage;

  static_assert(MAX_NUMA_NODES <= 256,
                "Not enough space to store nid in struct page");
  uint8_t nid;

  union data {
    uint8_t order;
    struct slab_data {
     // In slab allocator
      explicitly_constructed<boost::intrusive::list_member_hook<> > member_hook;
      explicitly_constructed<compact_free_object_list> list;
      SlabCache *cache;
      size_t used;
    } __attribute__((packed)) slab_data;
  } __attribute__((packed)) data;

  page(nid_t nid) : usage{ Usage::RESERVED }, nid{(uint8_t)nid } {}
} __attribute__((packed));

void mem_map_init();

class mem_section {
  uintptr_t mem_map;

  static const constexpr uintptr_t PRESENT = 1;
  static const constexpr uintptr_t HAS_MEM_MAP = 2;
  static const constexpr uintptr_t MAP_MASK = ~3;
  static const constexpr size_t NID_SHIFT = 2;
  page *map_addr() { return reinterpret_cast<page *>(mem_map & MAP_MASK); }
  friend void mem_map_init();
  void set_early_nid(nid_t nid) {
    mem_map = (((uintptr_t)nid) << NID_SHIFT) | PRESENT;
  }
  nid_t early_get_nid() { return nid_t(mem_map >> NID_SHIFT); }
  void set_map(uintptr_t map_addr, size_t index) {
    // set the addr to take into account the starting frame so get_page is more
    // efficient
    auto addr = map_addr - index * PAGES_PER_SECTION * sizeof(page);
    mem_map = addr | PRESENT | HAS_MEM_MAP;
  }

public:
  bool present() const { return mem_map & PRESENT; }
  bool valid() const { return mem_map & MAP_MASK; }
  page *get_page(pfn_t pfn) {
    // mem_map encodes the starting frame so we can just index off it
    // e.g.: mem_map = mem_map_address - start_pfn
    return map_addr() + pfn;
  }
};

extern std::array<mem_section, NUM_MEM_SECTIONS> sections;

inline size_t pfn_to_section_num(pfn_t pfn) { return pfn >> PFN_SECTION_SHIFT; }

inline mem_section &pfn_to_section(pfn_t pfn) {
  return sections[pfn_to_section_num(pfn)];
}

inline page *pfn_to_page(pfn_t pfn) {
  auto section = pfn_to_section(pfn);
  if (!section.valid() || !section.present()) {
    return nullptr;
  }
  return section.get_page(pfn);
}

inline page *addr_to_page(uintptr_t addr) {
  return pfn_to_page(pfn_down(addr));
}

inline page *addr_to_page(void* addr) {
  return addr_to_page(reinterpret_cast<uintptr_t>(addr));
}
}
