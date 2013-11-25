#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

#include <sys/multiboot.hpp>

namespace ebbrt {

struct e820_entry {
  uint64_t addr;
  uint64_t length;
  uint32_t type;

  static const constexpr uint32_t TYPE_RAM = 1;
  static const constexpr uint32_t TYPE_RESERVED_UNUSABLE = 2;
  static const constexpr uint32_t TYPE_ACPI_RECLAIMABLE = 3;
  static const constexpr uint32_t TYPE_ACPI_NVS = 4;
  static const constexpr uint32_t TYPE_BAD_MEMORY = 5;

  void trim_below(uint64_t new_addr) {
    if (addr > new_addr || addr + length < new_addr)
      return;

    length -= new_addr - addr;
    addr = new_addr;
  }

  void trim_above(uint64_t new_addr) {
    if (addr > new_addr || addr + length < new_addr)
      return;
    length -= (addr + length) - new_addr;
  }
};

inline bool operator==(const e820_entry &lhs, const e820_entry &rhs) {
  return lhs.addr == rhs.addr;
}

inline bool operator!=(const e820_entry &lhs, const e820_entry &rhs) {
  return !operator==(lhs, rhs);
}

inline bool operator<(const e820_entry &lhs, const e820_entry &rhs) {
  return lhs.addr < rhs.addr;
}

inline bool operator>(const e820_entry &lhs, const e820_entry &rhs) {
  return operator<(rhs, lhs);
}

inline bool operator<=(const e820_entry &lhs, const e820_entry &rhs) {
  return !operator>(lhs, rhs);
}

inline bool operator>=(const e820_entry &lhs, const e820_entry &rhs) {
  return !operator<(lhs, rhs);
}

const constexpr size_t MAX_NUM_E820_ENTRIES = 128;
extern std::array<e820_entry, MAX_NUM_E820_ENTRIES> e820_map;
extern size_t e820_num_entries;

void e820_init(MultibootInformation *mbi);

void e820_print_map();

void e820_reserve(uint64_t addr, uint64_t length);

template <typename F> void e820_for_each_usable_region(F f) {
  std::for_each(std::begin(e820_map), std::begin(e820_map) + e820_num_entries,
                [=](const e820_entry & entry) {
    if (entry.type == e820_entry::TYPE_RAM) {
      f(entry);
    }
  });
}

uint64_t e820_allocate_page();
}
