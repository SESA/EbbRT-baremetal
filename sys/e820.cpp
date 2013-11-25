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

// void ebbrt::e820_reserve(uint64_t addr, uint64_t length) {
//   for (size_t i = 0; i < e820_num_entries; ++i) {
//     // passed range
//     if (e820_map[i].addr >= addr + length)
//       return;

//     // still before range
//     if (e820_map[i].addr + e820_map[i].length <= addr)
//       continue;

//     if (e820_map[i].type != e820_entry::TYPE_RAM)
//       continue;

//     if (e820_map[i].addr >= addr &&
//         e820_map[i].addr + e820_map[i].length <= addr + length) {
//       // entry is completely in range to be reserved
//       e820_map[i].type = e820_entry::TYPE_RESERVED_KERNEL;
//       continue;
//     }

//     e820_entry *new_entry = nullptr;
//     if (e820_map[i].addr == addr) {
//       // case 0: region is at the beginning of entry
//       e820_map[i].addr = addr + length;
//       e820_map[i].length -= length;
//       kassert(e820_num_entries + 1 <= MAX_NUM_E820_ENTRIES);
//       std::move_backward(std::begin(e820_map) + i,
//                          std::begin(e820_map) + e820_num_entries,
//                          std::begin(e820_map) + e820_num_entries + 1);
//       ++e820_num_entries;
//       new_entry = &e820_map[i];
//     } else if (e820_map[i].addr + length == addr + length) {
//       // case 1: region is at the end of entry
//       e820_map[i].length -= length;
//       kassert(e820_num_entries + 1 <= MAX_NUM_E820_ENTRIES);
//       std::move_backward(std::begin(e820_map) + i + 1,
//                          std::begin(e820_map) + e820_num_entries,
//                          std::begin(e820_map) + e820_num_entries + 1);
//       ++e820_num_entries;
//       new_entry = &e820_map[i + 1];
//     } else {
//       // case 2: region is in the middle of entry
//       kassert(e820_num_entries + 2 <= MAX_NUM_E820_ENTRIES);
//       std::move_backward(std::begin(e820_map) + i + 1,
//                          std::begin(e820_map) + e820_num_entries,
//                          std::begin(e820_map) + e820_num_entries + 2);
//       e820_num_entries += 2;
//       e820_map[i + 2].addr = addr + length;
//       e820_map[i + 2].length =
//           (e820_map[i].addr + e820_map[i].length) - (addr + length);
//       e820_map[i].length = addr - e820_map[i].addr;
//       new_entry = &e820_map[i + 1];
//     }

//     new_entry->addr = addr;
//     new_entry->length = length;
//     new_entry->type = e820_entry::TYPE_RESERVED_KERNEL;
//   }
// }

// uint64_t ebbrt::e820_allocate_page() {
//   for (size_t i = 0; i < e820_num_entries; ++i) {
//     auto &entry = e820_map[i];

//     if (entry.type != e820_entry::TYPE_RAM)
//       continue;

//     auto aligned_addr = align_up(entry.addr, page_size);
//     if (aligned_addr + page_size > entry.addr + entry.length)
//       continue;

//     // make sure the page we give out is in the lower gig of memory because that
//     // is the amount we initially map
//     if (aligned_addr + page_size > (1 << 30)) {
//       break;
//     }

//     e820_reserve(aligned_addr, page_size);
//     return aligned_addr;
//   }
//   kprintf("e820: Failed to allocate page, aborting\n");
//   kabort();
// }
