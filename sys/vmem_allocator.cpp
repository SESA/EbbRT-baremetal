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
  regions_.emplace(pfn_up(0xFFFF800000000000),
                   pfn_down(LOCAL_TRANS_VMEM_START));
}

pfn_t VMemAllocator::Alloc(size_t npages, page_fault_handler_t pf_handler) {
  for (auto it = regions_.begin(); it != regions_.end(); ++it) {
    if (!it->free() || it->npages() < npages)
      continue;

    auto ret = it->end() - npages;

    if (ret == it->start()) {
      it->set_handler(std::move(pf_handler));
    } else {
      it->truncate(npages);
      auto p = regions_.emplace(ret, ret + npages);
      p.first->set_handler(std::move(pf_handler));
    }

    return ret;
  }
  kabort("%s: unable to allocate %llu virtual pages\n", __PRETTY_FUNCTION__,
         npages);
}

void VMemAllocator::HandlePageFault(exception_frame *ef) {
  auto fault_addr = read_cr2();
  auto it = regions_.lower_bound(region(pfn_down(fault_addr)));
  kbugon(it == regions_.end() || it->end() < pfn_up(fault_addr),
         "Could not find region for faulting address!\n");
  kbugon(it->free(), "Fault on a free region!\n");
  it->get_handler().handle_fault(ef, fault_addr);
}

extern "C" void page_fault_exception(exception_frame *ef) {
  vmem_allocator->HandlePageFault(ef);
}
