#pragma once

#include <cstdint>

namespace ebbrt {
namespace pmem {
constexpr uint64_t page_size = 4096;

void init();
uint64_t allocate_page(uint64_t size = page_size);
}
}
