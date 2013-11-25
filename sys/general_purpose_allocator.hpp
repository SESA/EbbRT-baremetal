#pragma once

#include <array>

#include <sys/cache_aligned.hpp>
#include <sys/debug.hpp>
#include <sys/slab_allocator.hpp>
#include <sys/trans.hpp>

namespace ebbrt {
template <size_t... sizes_in>
class general_purpose_allocator : public cache_aligned {
  std::array<SlabAllocator *, sizeof...(sizes_in)> allocators_;

  static std::array<SlabAllocatorRoot *, sizeof...(sizes_in)> allocator_roots;

  static std::array<general_purpose_allocator<sizes_in...> *, MAX_NUM_CPUS>
  reps;

  static SlabAllocatorRoot *rep_allocator;

  template <size_t index, size_t... tail> struct construct {
    void
    operator()(std::array<SlabAllocatorRoot *, sizeof...(sizes_in)> &roots) {}
  };

  template <size_t index, size_t head, size_t... tail>
  struct construct<index, head, tail...> {
    static_assert(head <= MAX_SLAB_SIZE, "gp allocator instantiation failed, "
                                         "request for slab allocator is too "
                                         "large");
    void
    operator()(std::array<SlabAllocatorRoot *, sizeof...(sizes_in)> &roots) {
      roots[index] = new SlabAllocatorRoot(head);
      construct<index + 1, tail...> next;
      next(roots);
    }
  };

  template <size_t index, size_t... tail> struct indexer {
    size_t operator()(size_t size) { return -1; }
  };

  template <size_t index, size_t head, size_t... tail>
  struct indexer<index, head, tail...> {
    size_t operator()(size_t size) {
      if (size <= head) {
        return index;
      } else {
        indexer<index + 1, tail...> i;
        return i(size);
      }
    }
  };

public:
  static void Init() {
    rep_allocator =
        new SlabAllocatorRoot(sizeof(general_purpose_allocator<sizes_in...>),
                              alignof(general_purpose_allocator<sizes_in...>));

    construct<0, sizes_in...> c;
    c(allocator_roots);
  }

  static general_purpose_allocator<sizes_in...> *HandleFault(EbbId id) {
    if (reps[my_cpu_index] == nullptr) {
      reps[my_cpu_index] = new general_purpose_allocator<sizes_in...>();
    }

    auto allocator = reps[my_cpu_index];
    cache_ref(id, allocator);
    return allocator;
  }

  general_purpose_allocator() {
    for (size_t i = 0; i < allocators_.size(); ++i) {
      allocators_[i] = &allocator_roots[i]->get_cpu_allocator();
    }
  }

  void *operator new(size_t size) {
    auto &allocator = rep_allocator->get_cpu_allocator();
    auto ret = allocator.Alloc();

    if (ret == nullptr)
      throw std::bad_alloc();

    return ret;
  }

  void operator delete(void *p) { UNIMPLEMENTED(); }

  void *Alloc(size_t size) {
    indexer<0, sizes_in...> i;
    auto index = i(size);
    kbugon(index == -1, "Attempt to allocate %zu bytes not supported\n", size);
    auto ret = allocators_[index]->Alloc();
    kbugon(ret == nullptr,
           "Failed to allocate from this NUMA node, should try others\n");
    return ret;
  }

  void Free(void* p) {
    auto page = addr_to_page(p);
    kassert(page != nullptr);

    auto& allocator = page->data.slab_data.cache->root_.get_cpu_allocator();
    allocator.Free(p);
  }
};

template <size_t... sizes_in>
std::array<SlabAllocatorRoot *, sizeof...(sizes_in)>
general_purpose_allocator<sizes_in...>::allocator_roots;

template <size_t... sizes_in>
std::array<general_purpose_allocator<sizes_in...> *, MAX_NUM_CPUS>
general_purpose_allocator<sizes_in...>::reps;

template <size_t... sizes_in>
SlabAllocatorRoot *general_purpose_allocator<sizes_in...>::rep_allocator;

// roughly equivalent to the breakdown Linux uses, may need tuning
typedef general_purpose_allocator<
    8, 16, 32, 64, 96, 128, 192, 256, 512, 1024, 2 * 1024, 4 * 1024, 8 * 1024,
    16 * 1024, 32 * 1024, 64 * 1024, 128 * 1024, 256 * 1024, 512 * 1024,
    1024 * 1024, 2 * 1024 * 1024, 4 * 1024 * 1024, 8 * 1024 * 1024> gp_type;

constexpr auto gp_allocator = EbbRef<gp_type>{ gp_allocator_id };
}
