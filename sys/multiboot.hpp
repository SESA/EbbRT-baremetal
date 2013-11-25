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
    uint32_t reserved : 20;
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

struct MultibootMemoryRegion {
  uint32_t size;
  uint64_t base_address;
  uint64_t length;
  uint32_t type;
  static const uint32_t RAM_TYPE = 1;
  static const uint32_t RESERVED_UNUSABLE = 2;
  static const uint32_t ACPI_RECLAIMABLE = 3;
  static const uint32_t ACPI_NVS = 4;
  static const uint32_t BAD_MEMORY = 5;

  template <typename F>
  static void for_each_mmregion(const void *buffer, uint32_t size, F f) {
    auto end_addr = static_cast<const char *>(buffer) + size;
    auto p = static_cast<const char *>(buffer);
    while (p < end_addr) {
      auto region = reinterpret_cast<const MultibootMemoryRegion *>(p);
      f(*region);
      p += region->size + 4;
    }
  }
} __attribute__((packed));
}
