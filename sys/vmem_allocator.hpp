#pragma once

#include <boost/container/flat_set.hpp>
#include <boost/optional.hpp>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#include <boost/type_erasure/any.hpp>
#include <boost/type_erasure/builtin.hpp>
#include <boost/type_erasure/member.hpp>
#pragma GCC diagnostic pop

#include <sys/cache_aligned.hpp>
#include <sys/idt.hpp>
#include <sys/pfn.hpp>
#include <sys/trans.hpp>

BOOST_TYPE_ERASURE_MEMBER((has_handle_fault), handle_fault, 2)
namespace ebbrt {
class VMemAllocator : cache_aligned {
public:
  typedef boost::mpl::vector<
      boost::type_erasure::copy_constructible<>, boost::type_erasure::relaxed,
      has_handle_fault<void(exception_frame *, uintptr_t)> > concepts;
  typedef boost::type_erasure::any<concepts> page_fault_handler_t;

private:

  class region {
    pfn_t start_;
    mutable pfn_t end_;
    mutable boost::optional<page_fault_handler_t> page_fault_handler_;

  public:
    explicit region(pfn_t addr) : region(addr, pfn_t(0)) {}
    region(pfn_t start, pfn_t end) : start_{ start }, end_{ end } {}
    pfn_t start() const { return start_; }
    pfn_t end() const { return end_; }
    size_t npages() const { return end_ - start_; }
    bool free() const { return !page_fault_handler_; }

    void truncate(size_t npages) const { end_ = end_ - npages; }
    void set_handler(page_fault_handler_t handler) {
      page_fault_handler_ = std::move(handler);
    }
    page_fault_handler_t& get_handler() {
      return page_fault_handler_.get();
    }
  };

  struct region_compare {
    bool operator()(const region &a, const region &b) const {
      return a.start() > b.start();
    }
  };

  boost::container::flat_set<region, region_compare> regions_;

  VMemAllocator();
  friend void ::page_fault_exception(ebbrt::exception_frame *ef);
  void HandlePageFault(exception_frame *ef);

public:
  static void Init();
  static VMemAllocator &HandleFault(EbbId id);

  pfn_t Alloc(size_t npages, page_fault_handler_t pf_handler);
};

constexpr auto vmem_allocator = EbbRef<VMemAllocator>{ vmem_allocator_id };
}
