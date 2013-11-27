#pragma once

#include <boost/container/static_vector.hpp>

#include <sys/numa.hpp>

namespace ebbrt {

class cpu {
  size_t index_;
  uint8_t acpi_id_;
  uint8_t apic_id_;
  nid_t nid_;

  friend void numa_init();
  void set_nid(nid_t nid) {
    nid_ = nid;
  }
public:
  cpu(size_t index, uint8_t acpi_id, uint8_t apic_id)
    : index_{index}, acpi_id_{ acpi_id }, apic_id_{ apic_id } {}

  operator size_t() {
    return index_;
  }
  uint8_t get_apic_id() {
    return apic_id_;
  }
  nid_t get_nid() {
    return nid_;
  }
};

const constexpr size_t MAX_NUM_CPUS = 256;

extern thread_local cpu* my_cpu_tls;

inline cpu& my_cpu() {
  return *my_cpu_tls;
}

extern boost::container::static_vector<cpu, MAX_NUM_CPUS> cpus;

inline nid_t my_node() {
  return cpus[my_cpu()].get_nid();
}

const constexpr uint32_t IA32_FS_BASE = 0xC0000100;

inline uint64_t rdmsr(uint32_t index) {
  uint32_t low, high;
  asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(index));
  return low | (uint64_t(high) << 32);
}

inline void wrmsr(uint32_t index, uint64_t data) {
  uint32_t low = data;
  uint32_t high = data >> 32;
  asm volatile("wrmsr" : : "c"(index), "a"(low), "d"(high));
}
}
