#pragma once

#include <cstdint>

namespace ebbrt {

const constexpr uintptr_t LOCAL_TRANS_VMEM_START = 0xFFFFFFFF00000000;

void trans_init();

struct LocalEntry {
  void *ref;
};

typedef uint32_t EbbId;

template <class T> class EbbRef {
  uintptr_t ref_;

public:
  constexpr explicit EbbRef(EbbId id)
      : ref_{ LOCAL_TRANS_VMEM_START + sizeof(LocalEntry) * id } {}

  T *operator->() const {
    auto lref = *reinterpret_cast<T **>(ref_);
    if (lref == nullptr) {
      auto id = (ref_ - LOCAL_TRANS_VMEM_START) / sizeof(LocalEntry);
      lref = T::HandleFault(id);
    }
    return lref;
  }
};

enum : EbbId {
  page_allocator_id,
  gp_allocator_id
};

template <typename T> inline void cache_ref(EbbId id, T *ref) {
  auto le = reinterpret_cast<LocalEntry *>(LOCAL_TRANS_VMEM_START +
                                           sizeof(LocalEntry) * id);
  le->ref = ref;
}
}
