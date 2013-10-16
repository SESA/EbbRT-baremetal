#include <cinttypes>

#include <sys/debug.hpp>
#include <sys/priority.hpp>

extern char event_entry[];

namespace ebbrt {
namespace idt {
extern "C" void int_de();
extern "C" void int_db();
extern "C" void int_nmi();
extern "C" void int_bp();
extern "C" void int_of();
extern "C" void int_br();
extern "C" void int_ud();
extern "C" void int_nm();
extern "C" void int_df();
extern "C" void int_ts();
extern "C" void int_np();
extern "C" void int_ss();
extern "C" void int_gp();
extern "C" void int_pf();
extern "C" void int_mf();
extern "C" void int_ac();
extern "C" void int_mc();
extern "C" void int_xm();

class entry {
  union {
    uint64_t raw_[2];
    struct {
      uint64_t offset_low_ : 16;
      uint64_t selector_ : 16;
      uint64_t ist_ : 3;
      uint64_t reserved0 : 5;
      uint64_t type_ : 4;
      uint64_t reserved1 : 1;
      uint64_t dpl_ : 2;
      uint64_t p_ : 1;
      uint64_t offset_high_ : 48;
      uint64_t reserved2 : 32;
    } __attribute__((packed));
  };

public:
  static constexpr uint64_t TYPE_INTERRUPT = 0xe;
  void set(uint16_t selector, void (*handler)(), uint64_t type, uint64_t pl,
           uint64_t ist) {
    selector_ = selector;
    offset_low_ = reinterpret_cast<uint64_t>(handler) & 0xFFFF;
    offset_high_ = reinterpret_cast<uint64_t>(handler) >> 16;
    ist_ = ist;
    type_ = type;
    dpl_ = pl;
    p_ = 1;
  }
} __attribute__((aligned(16)));

static_assert(sizeof(entry) == 16, "packing issue");

entry idt[256];
void load() {
  struct idtr {
    idtr(uint16_t size, uint64_t addr) : size(size), addr(addr) {}
    uint16_t size;
    uint64_t addr;
  } __attribute__((packed));

  auto p = idtr{ sizeof(idt) - 1, reinterpret_cast<uint64_t>(&idt) };
  asm volatile("lidt %[idt]" : : [idt] "m"(p));
}

void init() {
  uint16_t cs;
  asm volatile("mov %%cs, %[cs]" : [cs] "=r"(cs));
  idt[0].set(cs, int_de, entry::TYPE_INTERRUPT, 0, 1);
  idt[1].set(cs, int_db, entry::TYPE_INTERRUPT, 0, 1);
  idt[2].set(cs, int_nmi, entry::TYPE_INTERRUPT, 0, 1);
  idt[3].set(cs, int_bp, entry::TYPE_INTERRUPT, 0, 1);
  idt[4].set(cs, int_of, entry::TYPE_INTERRUPT, 0, 1);
  idt[5].set(cs, int_br, entry::TYPE_INTERRUPT, 0, 1);
  idt[6].set(cs, int_ud, entry::TYPE_INTERRUPT, 0, 1);
  idt[7].set(cs, int_nm, entry::TYPE_INTERRUPT, 0, 1);
  idt[8].set(cs, int_df, entry::TYPE_INTERRUPT, 0, 1);
  idt[10].set(cs, int_ts, entry::TYPE_INTERRUPT, 0, 1);
  idt[11].set(cs, int_np, entry::TYPE_INTERRUPT, 0, 1);
  idt[12].set(cs, int_ss, entry::TYPE_INTERRUPT, 0, 1);
  idt[13].set(cs, int_gp, entry::TYPE_INTERRUPT, 0, 1);
  idt[14].set(cs, int_pf, entry::TYPE_INTERRUPT, 0, 1);
  idt[16].set(cs, int_mf, entry::TYPE_INTERRUPT, 0, 1);
  idt[17].set(cs, int_ac, entry::TYPE_INTERRUPT, 0, 1);
  idt[18].set(cs, int_mc, entry::TYPE_INTERRUPT, 0, 1);
  idt[19].set(cs, int_xm, entry::TYPE_INTERRUPT, 0, 1);

  for (int i = 32; i < 256; ++i) {
    idt[i].set(cs, reinterpret_cast<void (*)()>(event_entry + (i - 32) * 16),
               entry::TYPE_INTERRUPT, 0, 1);
  }
}

extern "C" void event_interrupt(int num) {
  kprintf("Unhandled event %d\n", num);
  kabort();
}

struct exception_frame {
  uint64_t fpu[64];
  uint64_t empty;
  uint64_t r15;
  uint64_t r14;
  uint64_t r13;
  uint64_t r12;
  uint64_t r11;
  uint64_t r10;
  uint64_t r9;
  uint64_t r8;
  uint64_t rbp;
  uint64_t rdi;
  uint64_t rsi;
  uint64_t rdx;
  uint64_t rcx;
  uint64_t rbx;
  uint64_t rax;
  uint64_t error_code;
  uint64_t rip;
  uint64_t cs;
  uint64_t rflags;
  uint64_t rsp;
  uint64_t ss;
};

void print_exception_frame(exception_frame *ef) {
  kprintf("SS: %#018" PRIx64 " RSP: %#018" PRIx64 "\n", ef->ss, ef->rsp);
  kprintf("FLAGS: %#018" PRIx64 "\n",
          ef->rflags); // TODO: print out actual meaning
  kprintf("CS: %#018" PRIx64 " RIP: %#018" PRIx64 "\n", ef->cs, ef->rip);
  kprintf("Error Code: %" PRIx64 "\n", ef->error_code);
  kprintf("RAX: %#018" PRIx64 " RBX: %#018" PRIx64 "\n", ef->rax, ef->rbx);
  kprintf("RCX: %#018" PRIx64 " RDX: %#018" PRIx64 "\n", ef->rcx, ef->rdx);
  kprintf("RSI: %#018" PRIx64 " RDI: %#018" PRIx64 "\n", ef->rsi, ef->rdi);
  kprintf("RBP: %#018" PRIx64 " R8:  %#018" PRIx64 "\n", ef->rbp, ef->r8);
  kprintf("R9:  %#018" PRIx64 " R10: %#018" PRIx64 "\n", ef->r9, ef->r10);
  kprintf("R11: %#018" PRIx64 " R12: %#018" PRIx64 "\n", ef->r11, ef->r12);
  kprintf("R13: %#018" PRIx64 " R14: %#018" PRIx64 "\n", ef->r13, ef->r14);
  kprintf("R15: %#018" PRIx64 "\n", ef->r15);

  // TODO: FPU
}

extern "C" void nmi_interrupt(exception_frame *ef) { ebbrt::kabort(); }

#define UNHANDLED_INTERRUPT(name)                                              \
  extern "C" void name(exception_frame *ef) {                                  \
    kprintf("%s\n", __FUNCTION__);                                             \
    print_exception_frame(ef);                                                 \
    kabort();                                                                  \
  }

UNHANDLED_INTERRUPT(divide_error_exception)
UNHANDLED_INTERRUPT(debug_exception)
UNHANDLED_INTERRUPT(breakpoint_exception)
UNHANDLED_INTERRUPT(overflow_exception)
UNHANDLED_INTERRUPT(bound_range_exceeded_exception)
UNHANDLED_INTERRUPT(invalid_opcode_exception)
UNHANDLED_INTERRUPT(device_not_available_exception)
UNHANDLED_INTERRUPT(double_fault_exception)
UNHANDLED_INTERRUPT(invalid_tss_exception)
UNHANDLED_INTERRUPT(segment_not_present)
UNHANDLED_INTERRUPT(stack_fault_exception)
UNHANDLED_INTERRUPT(general_protection_exception)
UNHANDLED_INTERRUPT(page_fault_exception)
UNHANDLED_INTERRUPT(x86_fpu_floating_point_error)
UNHANDLED_INTERRUPT(alignment_check_exception)
UNHANDLED_INTERRUPT(machine_check_exception)
UNHANDLED_INTERRUPT(simd_floating_point_exception)
}
}
