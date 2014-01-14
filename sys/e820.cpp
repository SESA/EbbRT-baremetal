#include <cinttypes>

#include <sys/debug.hpp>
#include <sys/e820.hpp>

using namespace ebbrt;

std::array<e820_entry, MAX_NUM_E820_ENTRIES> ebbrt::e820_map;
size_t ebbrt::e820_num_entries;

void ebbrt::e820_init(MultibootInformation *mbi) {
  MultibootMemoryRegion::for_each_mmregion(
      reinterpret_cast<const void *>(mbi->memory_map_address),
      mbi->memory_map_length, [](const MultibootMemoryRegion & mmregion) {
        kassert(e820_num_entries < MAX_NUM_E820_ENTRIES);
        auto &entry = e820_map[e820_num_entries++];
        entry.addr = mmregion.base_address;
        entry.length = mmregion.length;
        entry.type = mmregion.type;
      });

  std::sort(std::begin(e820_map), std::begin(e820_map) + e820_num_entries);
}

void ebbrt::e820_print_map() {
  kprintf("e820_map:\n");
  std::for_each(std::begin(e820_map), std::begin(e820_map) + e820_num_entries,
                [](const e820_entry & entry) {
    kprintf("%#018" PRIx64 "-%#018" PRIx64 " ", entry.addr,
            entry.addr + entry.length - 1);
    switch (entry.type) {
    case e820_entry::TYPE_RAM:
      kprintf("usable");
      break;
    case e820_entry::TYPE_RESERVED_UNUSABLE:
      kprintf("reserved");
      break;
    case e820_entry::TYPE_ACPI_RECLAIMABLE:
      kprintf("ACPI data");
      break;
    case e820_entry::TYPE_ACPI_NVS:
      kprintf("ACPI NVS");
      break;
    case e820_entry::TYPE_BAD_MEMORY:
      kprintf("unusable");
      break;
    default:
      kprintf("type %" PRIu32, entry.type);
      break;
    }
    kprintf("\n");
  });
}
