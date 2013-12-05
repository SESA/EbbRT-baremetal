#pragma once

#include <boost/strong_typedef.hpp>

namespace ebbrt {
const constexpr size_t PAGE_SHIFT = 12;
const constexpr size_t PAGE_SIZE = 1 << PAGE_SHIFT;

BOOST_STRONG_TYPEDEF(uintptr_t, pfn_t);

#define NO_PFN                                                                 \
  pfn_t { UINTPTR_MAX }

inline pfn_t pfn_up(uintptr_t addr) {
  auto pfn = (addr + PAGE_SIZE - 1) >> PAGE_SHIFT;
  return pfn_t(pfn);
}

inline pfn_t pfn_down(uintptr_t addr) {
  auto pfn = addr >> PAGE_SHIFT;
  return pfn_t(pfn);
}

inline pfn_t pfn_up(void *addr) {
  return pfn_up(reinterpret_cast<uintptr_t>(addr));
}

inline pfn_t pfn_down(void *addr) {
  return pfn_down(reinterpret_cast<uintptr_t>(addr));
}

inline uintptr_t pfn_to_addr(pfn_t pfn) {
  return ((uintptr_t)pfn) << PAGE_SHIFT;
}

template <typename T,
          class = typename std::enable_if<std::is_integral<T>::value>::type>
pfn_t operator+(const pfn_t &lhs, T npages) {
  return pfn_t((uintptr_t)lhs + npages);
}

template <typename T,
          class = typename std::enable_if<std::is_integral<T>::value>::type>
pfn_t operator-(const pfn_t &lhs, T npages) {
  return pfn_t((uintptr_t)lhs - npages);
}
}

namespace std {
template <> struct hash<ebbrt::pfn_t> {
  size_t operator()(const ebbrt::pfn_t &x) const {
    return hash<uintptr_t>()((uintptr_t)x);
  }
};
}
