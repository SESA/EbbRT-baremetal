#pragma once

#include <atomic>
#include <memory>

#include <boost/intrusive/parent_from_member.hpp>

#include <sys/align.hpp>
#include <sys/cache_aligned.hpp>
#include <sys/cpu.hpp>
#include <sys/fls.hpp>
#include <sys/mem_map.hpp>
#include <sys/numa.hpp>
#include <sys/page_allocator.hpp>
#include <sys/slab_object.hpp>
#include <sys/spinlock.hpp>
#include <sys/trans.hpp>

namespace ebbrt {

void slab_init();

struct SlabAllocatorRoot;

class SlabCache {
public:
  struct remote : public cache_aligned {
    spinlock lock;
    free_object_list list;
  } remote_;

private:
  struct PageHookFunctor {
    typedef boost::intrusive::list_member_hook<> hook_type;
    typedef hook_type *hook_ptr;
    typedef const hook_type *const_hook_ptr;
    typedef page value_type;
    typedef value_type *pointer;
    typedef const value_type *const_pointer;

    static hook_ptr to_hook_ptr(value_type &value) {
      return &(*value.data.slab_data.member_hook);
    }

    static const_hook_ptr to_hook_ptr(const value_type &value) {
      return &(*value.data.slab_data.member_hook);
    }

    static pointer to_value_ptr(hook_ptr n) {
      auto explicit_ptr = static_cast<explicitly_constructed<hook_type> *>(
          static_cast<void *>(n));
      auto slab_data_ptr = boost::intrusive::get_parent_from_member(
          explicit_ptr, &page::data::slab_data::member_hook);
      auto data_ptr = boost::intrusive::get_parent_from_member(
          slab_data_ptr, &page::data::slab_data);
      return boost::intrusive::get_parent_from_member(data_ptr, &page::data);
    }

    static const_pointer to_value_ptr(const_hook_ptr n) {
      auto explicit_ptr =
          static_cast<const explicitly_constructed<hook_type> *>(
              static_cast<const void *>(n));
      auto slab_data_ptr = boost::intrusive::get_parent_from_member(
          explicit_ptr, &page::data::slab_data::member_hook);
      auto data_ptr = boost::intrusive::get_parent_from_member(
          slab_data_ptr, &page::data::slab_data);
      return boost::intrusive::get_parent_from_member(data_ptr, &page::data);
    }
  };

  typedef boost::intrusive::list<
      page, boost::intrusive::function_hook<PageHookFunctor> > page_list;

  free_object_list object_list_;
  page_list partial_page_list_;

public:
  SlabAllocatorRoot &root_;
  std::atomic<bool> remote_check;

  SlabCache(SlabAllocatorRoot &root);
  ~SlabCache();

  void *Alloc();
  void Free(void *p);
  void AddSlab(pfn_t pfn);
  void FlushFreeList(size_t amount);
  void FlushFreeListAll();
  void ClaimRemoteFreeList();
};

class SlabAllocator : cache_aligned {
  SlabCache cache_;

  free_object_list remote_list_;
  SlabCache *remote_cache_;

  friend class SlabCache;
  friend class SlabAllocatorRoot;
  void FreeRemote(void *p);
  void FlushRemoteList();

public:
  static EbbRef<SlabAllocator> Construct(size_t size);
  static SlabAllocator& HandleFault(EbbId id);
  SlabAllocator(SlabAllocatorRoot &root);
  void *operator new(size_t size, nid_t nid);
  void operator delete(void *p);

  void *Alloc(nid_t nid = my_node());
  void Free(void *p);
};

class SlabAllocatorNode : cache_aligned {
  SlabCache cache_;
  nid_t nid_;
  spinlock lock_;

  friend class SlabAllocator;
  friend class SlabAllocatorRoot;
  void *Alloc();

  void *operator new(size_t size, nid_t nid);
  void operator delete(void *p);

public:
  SlabAllocatorNode(SlabAllocatorRoot &root, nid_t nid);
};

struct SlabAllocatorRoot {
  size_t align;
  size_t size;
  size_t order;
  size_t free_batch;
  size_t hiwater;

  // TODO: atomic_unique_ptr?
  std::array<std::atomic<SlabAllocatorNode *>, MAX_NUMA_NODES> node_allocators;
  std::array<std::unique_ptr<SlabAllocator>, MAX_NUM_CPUS> cpu_allocators;
  SlabAllocatorRoot(size_t size, size_t align = 0);
  ~SlabAllocatorRoot();

  void *operator new(size_t size);
  void operator delete(void *p);
  size_t num_objects_per_slab();
  SlabAllocator &get_cpu_allocator(size_t cpu_index = my_cpu());
  SlabAllocatorNode &get_node_allocator(nid_t nid);
};

const constexpr size_t MAX_SLAB_SIZE =
    1 << (PAGE_SHIFT + PageAllocator::MAX_ORDER);
}
