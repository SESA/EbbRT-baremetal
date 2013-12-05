#include <sys/cpu.hpp>
#include <sys/local_id_map.hpp>
#include <sys/vmem_allocator.hpp>

using namespace ebbrt;

void VMemAllocator::Init() {
  auto rep = new VMemAllocator;
  local_id_map->insert(
      std::make_pair(vmem_allocator_id, boost::any(std::ref(*rep))));
}

VMemAllocator &VMemAllocator::HandleFault(EbbId id) {
  kassert(id == vmem_allocator_id);
  LocalIdMap::const_accessor accessor;
  auto found = local_id_map->find(accessor, id);
  kassert(found);
  auto ref =
      boost::any_cast<std::reference_wrapper<VMemAllocator> >(accessor->second);
  cache_ref(id, ref.get());
  return ref;
}

VMemAllocator::VMemAllocator() {
  regions_.emplace(std::piecewise_construct,
                   std::forward_as_tuple(pfn_up(0xFFFF800000000000)),
                   std::forward_as_tuple(pfn_down(LOCAL_TRANS_VMEM_START)));
}

pfn_t VMemAllocator::Alloc(size_t npages,
                           std::unique_ptr<page_fault_handler_t> pf_handler) {
  std::lock_guard<spinlock> lock{ lock_ };
  for (auto it = regions_.begin(); it != regions_.end(); ++it) {
    const auto &begin = it->first;
    auto &end = it->second.end;
    if (it->second.page_fault_handler || end - begin < npages)
      continue;

    auto ret = end - npages;

    if (ret == begin) {
      it->second.page_fault_handler = std::move(pf_handler);
    } else {
      end -= npages;
      auto p =
          regions_.emplace(std::piecewise_construct, std::forward_as_tuple(ret),
                           std::forward_as_tuple(ret + npages));
      p.first->second.page_fault_handler = std::move(pf_handler);
    }

    return ret;
  }
  kabort("%s: unable to allocate %llu virtual pages\n", __PRETTY_FUNCTION__,
         npages);
}

void VMemAllocator::HandlePageFault(exception_frame *ef) {
  std::lock_guard<spinlock> lock{ lock_ };
  auto fault_addr = read_cr2();
  auto it = regions_.lower_bound(pfn_down(fault_addr));
  kbugon(it == regions_.end() || it->second.end < pfn_up(fault_addr),
         "Could not find region for faulting address!\n");
  kbugon(!it->second.page_fault_handler, "Fault on a free region!\n");
  it->second.page_fault_handler->handle_fault(ef, fault_addr);
}

extern "C" void page_fault_exception(exception_frame *ef) {
  vmem_allocator->HandlePageFault(ef);
}
