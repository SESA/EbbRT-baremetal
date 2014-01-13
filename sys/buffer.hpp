#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#include <boost/container/slist.hpp>
#pragma GCC diagnostic pop

#include <cstdlib>

#include <memory>
#include <new>

namespace ebbrt {
struct free_deleter {
  void operator()(void* p) const { std::free(p); }
};

class mutable_buffer {
  void* ptr_;
  size_t size_;
  size_t offset_;

 public:
  explicit mutable_buffer(std::size_t size) : size_(size), offset_(0) {
    auto mem = std::malloc(size);
    if (mem == nullptr)
      throw std::bad_alloc();
    ptr_ = mem;
  }

  mutable_buffer(void* ptr, std::size_t size)
      : ptr_(ptr), size_(size), offset_(0) {}

  mutable_buffer(const mutable_buffer&) = delete;
  mutable_buffer& operator=(const mutable_buffer&) = delete;

  mutable_buffer(mutable_buffer&&) = default;
  mutable_buffer& operator=(mutable_buffer&&) = default;

  ~mutable_buffer() {
    if (ptr_ == nullptr)
      return;

    auto p = static_cast<void*>(static_cast<char*>(ptr_) - offset_);
    free(p);
  }

  mutable_buffer& operator+=(size_t sz) {
    ptr_ = static_cast<void *>(static_cast<char *>(ptr_) + sz);
    size_ -= sz;
    offset_ += sz;
    return *this;
  }

  void* addr() const { return ptr_; }
  size_t size() { return size_; }

  std::pair<void*, size_t> release() {
    kassert(ptr_ != nullptr);
    auto p = static_cast<void*>(static_cast<char*>(ptr_) - offset_);
    ptr_ = nullptr;
    return std::make_pair(p, size_ + offset_);
  }
};

typedef boost::container::slist<mutable_buffer> mutable_buffer_list;

struct no_deleter {
  void operator()(const void* p) const {}
};

class const_buffer {
  std::shared_ptr<const void> ptr_;
  size_t size_;
  size_t offset_;

 public:
  template <typename Deleter = no_deleter>
  const_buffer(const void* ptr, std::size_t size, Deleter&& d = Deleter())
      : size_(size), offset_(0) {
    ptr_ = std::shared_ptr<const void>(ptr, std::forward<Deleter>(d));
  }

  const_buffer(const const_buffer&) = default;
  const_buffer& operator=(const const_buffer&) = default;

  const_buffer(const_buffer&&) = default;
  const_buffer& operator=(const_buffer&&) = default;

  const void* addr() const {
    auto p = ptr_.get();
    return static_cast<const void*>(static_cast<const char*>(p) + offset_);
  }

  size_t size() const { return size_; }
};

typedef boost::container::slist<const_buffer> const_buffer_list;
}
