#pragma once

#include <map>

#include <sys/cache_aligned.hpp>
#include <sys/idt.hpp>
#include <sys/pfn.hpp>
#include <sys/spinlock.hpp>
#include <sys/trans.hpp>

namespace ebbrt {
class VMemAllocator : cache_aligned {
public:
  class page_fault_handler_t {
  public:
    virtual void handle_fault(exception_frame *, uintptr_t) = 0;
    virtual ~page_fault_handler_t() {}
  };

private:

  struct region {
    pfn_t end;
    std::shared_ptr<page_fault_handler_t> page_fault_handler;

    explicit region(pfn_t addr) : end{ addr } {}
    bool free() { return static_cast<bool>(page_fault_handler); }
  };

  spinlock lock_;
  std::map<pfn_t, region, std::greater<pfn_t> > regions_;

  VMemAllocator();
  friend void ::page_fault_exception(ebbrt::exception_frame *ef);
  void HandlePageFault(exception_frame *ef);

public:
  static void Init();
  static VMemAllocator &HandleFault(EbbId id);

  pfn_t Alloc(size_t npages, std::unique_ptr<page_fault_handler_t> pf_handler);
};

constexpr auto vmem_allocator = EbbRef<VMemAllocator>{ vmem_allocator_id };
}
