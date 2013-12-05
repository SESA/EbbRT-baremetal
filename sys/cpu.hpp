#pragma once

#include <boost/container/static_vector.hpp>

#include <sys/apic.hpp>
#include <sys/debug.hpp>
#include <sys/explicitly_constructed.hpp>
#include <sys/idt.hpp>
#include <sys/numa.hpp>

namespace ebbrt {

class segdesc {
  union {
    uint64_t raw_;
    struct {
      uint64_t limit_low_ : 16;
      uint64_t base_low_ : 24;
      uint64_t type_ : 4;
      uint64_t s_ : 1;
      uint64_t dpl_ : 2;
      uint64_t p_ : 1;
      uint64_t limit_high_ : 4;
      uint64_t avl_ : 1;
      uint64_t l_ : 1;
      uint64_t db_ : 1;
      uint64_t g_ : 1;
      uint64_t base_high_ : 8;
    };
  };

public:
  segdesc() { clear(); }
  void clear() { raw_ = 0; }

  void set(uint8_t type, bool longmode) {
    type_ = type;
    l_ = longmode;
    s_ = 1;
    p_ = 1;
  }

  void set_code() { set(8, true); }

  void set_data() { set(0, false); }
} __attribute__((packed));

static_assert(sizeof(segdesc) == 8, "segdesc packing issue");

class tssdesc {
  union {
    uint64_t raw_[2];
    struct {
      uint64_t limit_low : 16;
      uint64_t base_low : 24;
      uint64_t type : 4;
      uint64_t reserved0 : 1;
      uint64_t dpl : 2;
      uint64_t p : 1;
      uint64_t limit_high : 4;
      uint64_t avl : 1;
      uint64_t reserved1 : 2;
      uint64_t g : 1;
      uint64_t base_high : 40;
      uint64_t reserved2 : 32;
    } __attribute__((packed));
  };

public:
  tssdesc() { clear(); }

  void clear() {
    raw_[0] = 0;
    raw_[1] = 0;
  }

  void set(uint64_t addr) {
    limit_low = 103;
    base_low = addr;
    base_high = addr >> 24;
    p = true;
    type = 9;
  }
};

static_assert(sizeof(tssdesc) == 16, "tssdesc packing issue");

class task_state_segment {
  union {
    uint64_t raw_[13];
    struct {
      uint32_t reserved0;
      uint64_t rsp[3];
      uint64_t reserved1;
      uint64_t ist[7];
      uint64_t reserved2;
      uint16_t reserved3;
      uint16_t io_map_base;
    } __attribute__((packed));
  };

public:
  void set_ist_entry(size_t entry, uint64_t addr) { ist[entry - 1] = addr; }
} __attribute__((packed));

static_assert(sizeof(task_state_segment) == 104, "tss packing issue");

struct aligned_tss {
  uint32_t padding;
  task_state_segment tss;
} __attribute__((packed, aligned(8)));

class gdt {
  segdesc nulldesc;
  segdesc cs;
  segdesc ds;
  tssdesc tss;

  struct desc_ptr {
    desc_ptr(uint16_t limit, uint64_t addr) : limit(limit), addr(addr) {}
    uint16_t limit;
    uint64_t addr;
  } __attribute__((packed));

public:
  gdt() {
    cs.set_code();
    ds.set_data();
  }

  void load() {
    auto gdtr = desc_ptr{ sizeof(gdt) - 1, reinterpret_cast<uint64_t>(this) };
    asm volatile("lgdt %[gdtr]" : : [gdtr] "m"(gdtr));
    asm volatile("ltr %w[tr]" : : [tr] "rm"(offsetof(class gdt, tss)));
  }

  void set_tss_addr(uint64_t tss_addr) { tss.set(tss_addr); }
};

struct cpu;
extern thread_local cpu *my_cpu_tls;

class cpu {
  aligned_tss atss_;
  gdt gdt_;
  size_t index_;
  uint8_t acpi_id_;
  uint8_t apic_id_;
  nid_t nid_;

  static char boot_interrupt_stack[PAGE_SIZE];
  friend void numa_init();
  void set_nid(nid_t nid) { nid_ = nid; }

public:
  cpu(size_t index, uint8_t acpi_id, uint8_t apic_id)
      : index_{ index }, acpi_id_{ acpi_id }, apic_id_{ apic_id } {}

  void init();

  operator size_t() const { return index_; }
  uint8_t get_apic_id() const { return apic_id_; }
  nid_t get_nid() const { return nid_; }

  void set_acpi_id(uint8_t id) { acpi_id_ = id; }
  void set_apic_id(uint8_t id) { apic_id_ = id; }
};

const constexpr size_t MAX_NUM_CPUS = 256;

inline cpu &my_cpu() { return *my_cpu_tls; }

extern boost::container::static_vector<cpu, MAX_NUM_CPUS> cpus;

inline nid_t my_node() { return cpus[my_cpu()].get_nid(); }

const constexpr uint32_t MSR_IA32_APIC_BASE = 0x0000001b;
const constexpr uint32_t MSR_KVM_PV_EOI = 0x4b564d04;
const constexpr uint32_t MSR_IA32_FS_BASE = 0xC0000100;

inline uintptr_t read_cr2() {
  uintptr_t cr2;
  asm volatile("mov %%cr2, %[cr2]" : [cr2] "=r"(cr2));
  return cr2;
}

inline uintptr_t read_cr3() {
  uintptr_t cr3;
  asm volatile("mov %%cr3, %[cr3]" : [cr3] "=r"(cr3));
  return cr3;
}

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
