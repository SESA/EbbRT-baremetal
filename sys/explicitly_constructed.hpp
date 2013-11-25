#pragma once

#include <type_traits>
#include <utility>

namespace ebbrt {
template <typename T> class explicitly_constructed {
  typename std::aligned_storage<sizeof(T), alignof(T)>::type storage_;

public:
  template <typename... Args> void construct(Args &&... args) {
    ::new (&storage_) T(std::forward<Args>(args)...);
  }
  void destruct() {
    auto ptr = static_cast<T *>(static_cast<void *>(&storage_));
    ptr->~T();
  }

  const T *operator->() const {
    return static_cast<T const *>(static_cast<void const *>(&storage_));
  }
  T *operator->() { return static_cast<T *>(static_cast<void *>(&storage_)); }

  const T &operator*() const {
    return *static_cast<T const *>(static_cast<void const *>(&storage_));
  }

  T &operator*() { return *static_cast<T *>(static_cast<void *>(&storage_)); }
  explicitly_constructed() = default;
  explicitly_constructed(const explicitly_constructed &) = delete;
  explicitly_constructed &operator=(const explicitly_constructed &) = delete;
  explicitly_constructed(explicitly_constructed &&) = delete;
  explicitly_constructed &operator=(const explicitly_constructed &&) = delete;
} __attribute__((packed));
}
