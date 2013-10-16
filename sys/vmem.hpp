#pragma once

namespace ebbrt {
namespace vmem {
  uint64_t allocate_page(uint64_t size);
  void free_page_range(uint64_t begin, uint64_t len);
  void map(uint64_t virt_addr, uint64_t phys_addr, uint64_t size);
  void enable_runtime_page_table();
  void init();
}
}
