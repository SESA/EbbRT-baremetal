#include <sys/align.hpp>
#include <sys/early_page_allocator.hpp>
#include <sys/mem_map.hpp>
#include <sys/numa.hpp>

using namespace ebbrt;

std::array<mem_section, NUM_MEM_SECTIONS> ebbrt::sections;

void ebbrt::mem_map_init() {
  for (const auto &node : numa_nodes) {
    for (const auto &memblock : node.memblocks) {
      auto nid = memblock.nid;
      auto start = memblock.start;
      auto end = memblock.end;

      start &= PAGE_SECTION_MASK;
      for (auto pfn = start; pfn < end; pfn += PAGES_PER_SECTION) {
        auto &section = pfn_to_section(pfn);
        if (!section.present()) {
          section.set_early_nid(nid);
        }
      }
    }
  }

  const auto section_map_bytes = PAGES_PER_SECTION * sizeof(page);
  const auto section_map_pages =
      align_up(section_map_bytes, PAGE_SIZE) >> PAGE_SHIFT;
  for (unsigned i = 0; i < sections.size(); ++i) {
    auto &section = sections[i];
    if (section.present()) {
      auto nid = section.early_get_nid();
      auto map_pfn = early_allocate_page(section_map_pages, nid);
      auto map_addr = pfn_to_addr(map_pfn);
      auto map = reinterpret_cast<page*>(map_addr);
      for (unsigned j = 0; j < PAGES_PER_SECTION; j++) {
        new (reinterpret_cast<void*>(&map[j])) page(nid);
      }
      section.set_map(map_addr, i);
    }
  }
}
