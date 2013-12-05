#include <atomic>

#include <sys/cpu.hpp>
#include <sys/debug.hpp>
#include <sys/early_page_allocator.hpp>
#include <sys/page_allocator.hpp>
#include <sys/trans.hpp>
#include <sys/vmem.hpp>

void ebbrt::trans_init() {
  auto page = early_allocate_page(1, my_node());
  traverse_page_table(
      page_table_root, LOCAL_TRANS_VMEM_START,
      LOCAL_TRANS_VMEM_START + PAGE_SIZE, 0, 4,
      [=](pte & entry, uint64_t base_virt, size_t level) {
        kassert(!entry.present());
        entry.set(pfn_to_addr(page) + (base_virt - LOCAL_TRANS_VMEM_START),
                  level > 0);
        std::atomic_thread_fence(std::memory_order_release);
        asm volatile("invlpg (%[addr])"
                     :
                     : [addr] "r"(base_virt)
                     : "memory");
      },
      [](pte & entry) {
        auto page = early_allocate_page(1, my_node());
        auto page_addr = pfn_to_addr(page);
        new (reinterpret_cast<void *>(page_addr)) pte[512];
        entry.set_normal(page_addr);
        return true;
      });
  std::memset(reinterpret_cast<void *>(pfn_to_addr(page)), 0, PAGE_SIZE);
}

void ebbrt::trans_ap_init(size_t index) {
  auto pte_root = pte{ read_cr3() };
  auto idx = pt_index(LOCAL_TRANS_VMEM_START, 3);
  auto pt = reinterpret_cast<pte *>(pte_root.addr(false));
  pt[idx].clear();

  auto nid = cpus[index].get_nid();
  auto &p_allocator = PageAllocator::allocators[nid];

  traverse_page_table(
      pte_root, LOCAL_TRANS_VMEM_START, LOCAL_TRANS_VMEM_START + PAGE_SIZE, 0,
      4,
      [&](pte & entry, uint64_t base_virt, size_t level) {
        kassert(!entry.present());
        auto page = p_allocator.Alloc(0, nid);
        std::memset(reinterpret_cast<void *>(pfn_to_addr(page)), 0, PAGE_SIZE);
        entry.set(pfn_to_addr(page) + (base_virt - LOCAL_TRANS_VMEM_START),
                  level > 0);
        std::atomic_thread_fence(std::memory_order_release);
        asm volatile("invlpg (%[addr])"
                     :
                     : [addr] "r"(base_virt)
                     : "memory");
      },
      [&](pte & entry) {
        auto page = p_allocator.Alloc(0, nid);
        auto page_addr = pfn_to_addr(page);
        new (reinterpret_cast<void *>(page_addr)) pte[512];
        entry.set_normal(page_addr);
        return true;
      });
}
