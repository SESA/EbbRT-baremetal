#pragma once

#include <type_traits>

namespace ebbrt {
template <typename T>
constexpr typename std::remove_reference<T>::type makeprval(T &&t) {
  return t;
}

#define isconstexpr(e) noexcept(makeprval(e))

template <typename T> constexpr T const &constexpr_max(T const &a, T const &b) {
  return a > b ? a : b;
}

template <typename T> constexpr T const &constexpr_min(T const &a, T const &b) {
  return a < b ? a : b;
}
}
