#pragma once

#include <cstddef>
#include <cstdint>

namespace ebbrt {
struct MultibootInformation {
  struct {
    uint32_t has_mem : 1;
    uint32_t has_boot_device : 1;
    uint32_t has_command_line : 1;
    uint32_t has_boot_modules : 1;
    uint32_t has_aout_symbol_table : 1;
    uint32_t has_elf_section_table : 1;
    uint32_t has_mem_map : 1;
    uint32_t has_drives : 1;
    uint32_t has_rom_configuration_table : 1;
    uint32_t has_boot_loader_name : 1;
    uint32_t has_apm_table : 1;
    uint32_t has_graphics_table : 1;
  uint32_t:
    20;
  };
  uint32_t memory_lower;
  uint32_t memory_higher;
  uint32_t boot_device;
  uint32_t command_line;
  uint32_t modules_count;
  uint32_t modules_address;
  uint32_t symbols[4];
  uint32_t memory_map_length;
  uint32_t memory_map_address;
  uint32_t drives_length;
  uint32_t drives_address;
  uint32_t configuration_table;
  uint32_t boot_loader_name;
  uint32_t apm_table;
  uint32_t vbe_control_info;
  uint32_t vbe_mode_info;
  uint32_t vbe_mode;
  uint32_t vbe_interface_segment;
  uint32_t vbe_interface_offset;
  uint32_t vbe_interface_length;
};

extern MultibootInformation saved_mbi;

class MultibootMemoryRegion {
  uint32_t size_;
  uint64_t base_address_;
  uint64_t length_;
  uint32_t type_;
  static const uint32_t RAM_TYPE = 1;

public:
  inline uint64_t begin() const noexcept {
    return base_address_;
  }
  inline uint64_t end() const noexcept {
    return base_address_ + length_;
  }
  inline uint64_t length() const noexcept {
    return length_;
  }

  inline bool isRAM() const noexcept { return type_ == RAM_TYPE; }

  inline bool intersects(uintptr_t addr) const noexcept {
    return addr > base_address_ && addr < base_address_ + length_;
  }

  inline void trim_below(uintptr_t addr) noexcept {
    if (intersects(addr)) {
      auto delta = addr - base_address_;
      base_address_ += delta;
      length_ -= delta;
    }
  }

  inline void trim_above(uintptr_t addr) noexcept {
    if (intersects(addr)) {
      auto delta = base_address_ + length_ - addr;
      length_ -= delta;
    }
  }

  static void for_each_mmregion(char *buffer, uint32_t size,
                                void (*f)(MultibootMemoryRegion mmregion)) {
    auto p = buffer;
    while (p < buffer + size) {
      auto region = reinterpret_cast<MultibootMemoryRegion *>(p);
      if (region->isRAM()) {
        f(*region);
      }
      p += region->size_ + 4;
    }
  }

} __attribute__((packed));

class MultibootModule {
public:
  uint32_t start_address;
  uint32_t end_address;
  char string[0];
};
}
