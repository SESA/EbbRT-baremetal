#pragma once

#include <algorithm>

namespace ebbrt {
class pte {
  uint64_t raw_;

public:
  pte() { clear(); }
  explicit pte(uint64_t raw) : raw_(raw) {}
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

inline size_t pt_index(uintptr_t virt_addr, size_t level) {
  return (virt_addr >> (12 + level * 9)) & ((1 << 9) - 1);
}

inline uint64_t canonical(uint64_t addr) {
  // sign extend for x86 canonical address
  return (int64_t(addr) << 16) >> 16;
}

extern pte page_table_root;
const constexpr size_t num_page_sizes = 2;

template <typename Found_Entry_Func, typename Empty_Entry_Func>
void traverse_page_table(pte &entry, uint64_t virt_start, uint64_t virt_end,
                         uint64_t base_virt, size_t level,
                         Found_Entry_Func found, Empty_Entry_Func empty) {
  if (!entry.present())
    if (!empty(entry))
      return;

  --level;
  auto pt = reinterpret_cast<pte *>(entry.addr(false));
  auto step = UINT64_C(1) << (12 + level * 9);
  auto idx_begin = pt_index(std::max(virt_start, base_virt), level);
  auto base_virt_end = canonical(base_virt + step * 512 - 1);
  auto idx_end = pt_index(std::min(virt_end - 1, base_virt_end), level);
  base_virt += canonical(idx_begin * step);
  for (size_t idx = idx_begin; idx <= idx_end; ++idx) {
    if (level < num_page_sizes && virt_start <= base_virt &&
        virt_end >= base_virt + step) {
      found(pt[idx], base_virt, level);
    } else {
      traverse_page_table(pt[idx], virt_start, virt_end, base_virt, level,
                          found, empty);
    }
    base_virt = canonical(base_virt + step);
  }
}

void enable_runtime_page_table();
void early_map_memory(uint64_t addr, uint64_t length);
void early_unmap_memory(uint64_t addr, uint64_t length);
void map_memory(pfn_t vfn, pfn_t pfn, uint64_t length = PAGE_SIZE);
void vmem_ap_init(size_t index);
}
