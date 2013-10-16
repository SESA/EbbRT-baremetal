#pragma once

#include <cstdint>

namespace ebbrt {
inline void out8(uint16_t port, uint8_t val) noexcept {
  asm volatile("outb %b[val], %w[port]"
               : // no output
               : [val] "a"(val), [port] "Nd"(port));
}
inline void out16(uint16_t port, uint16_t val) noexcept {
  asm volatile("outw %w[val], %w[port]"
               : // no output
               : [val] "a"(val), [port] "Nd"(port));
}
inline void out32(uint16_t port, uint32_t val) noexcept {
  asm volatile("outl %[val], %w[port]"
               : // no output
               : [val] "a"(val), [port] "Nd"(port));
}

inline uint8_t in8(uint16_t port) noexcept {
  uint8_t val;
  asm volatile("inb %w[port], %b[val]" : [val] "=a"(val) : [port] "Nd"(port));
  return val;
}

inline uint16_t in16(uint16_t port) noexcept {
  uint16_t val;
  asm volatile("inw %w[port], %w[val]" : [val] "=a"(val) : [port] "Nd"(port));
  return val;
}

inline uint32_t in32(uint16_t port) noexcept {
  uint32_t val;
  asm volatile("inl %w[port], %[val]" : [val] "=a"(val) : [port] "Nd"(port));
  return val;
}
}
