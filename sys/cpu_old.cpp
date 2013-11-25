#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
#include <xmmintrin.h>

#include <sys/apic.hpp>
#include <sys/idt.hpp>
#include <sys/pmem.hpp>
#include <sys/priority.hpp>
namespace ebbrt {
namespace cpu {
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

class cpu {
  aligned_tss atss;
  gdt gdt_;
  apic_t apic;

public:
  cpu() {
    gdt_.set_tss_addr(reinterpret_cast<uint64_t>(&atss.tss));
    auto pmem = pmem::allocate_page();
    atss.tss.set_ist_entry(1, pmem + pmem::page_size);
    gdt_.load();
    idt::load();
    _mm_setcsr(0x1f80); // default value
  }
};

std::vector<cpu> cpus __attribute__((init_priority(CPUS_INIT_PRIORITY)));

void init() { cpus.emplace_back(); }
}
}
