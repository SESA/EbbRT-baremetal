#pragma once

#include <cstdint>
#include <functional>

#include <boost/optional.hpp>

namespace ebbrt {
void pci_init();

class pci_function {
protected:
  static const constexpr uint8_t VENDOR_ID_ADDR = 0x00;
  static const constexpr uint8_t DEVICE_ID_ADDR = 0x02;
  static const constexpr uint8_t COMMAND_ADDR = 0x04;
  static const constexpr uint8_t STATUS_ADDR = 0x06;
  static const constexpr uint8_t REVISION_ID_ADDR = 0x08;
  static const constexpr uint8_t PROG_IF_ADDR = 0x09;
  static const constexpr uint8_t SUBCLASS_ADDR = 0x0A;
  static const constexpr uint8_t CLASS_CODE_ADDR = 0x0B;
  static const constexpr uint8_t CACHE_LINE_SIZE_ADDR = 0x0C;
  static const constexpr uint8_t LATENCY_TIMER_ADDR = 0x0D;
  static const constexpr uint8_t HEADER_TYPE_ADDR = 0x0E;
  static const constexpr uint8_t BIST_ADDR = 0x0F;

  static const constexpr uint16_t COMMAND_BUS_MASTER = 1 << 2;
  static const constexpr uint16_t COMMAND_INT_DISABLE = 1 << 10;

  static const constexpr uint8_t HEADER_MULTIFUNC_MASK = 0x80;
  static const constexpr uint8_t HEADER_TYPE_BRIDGE = 0x01;

  uint8_t bus_;
  uint8_t device_;
  uint8_t func_;

  uint8_t read8(uint8_t offset) const;
  uint16_t read16(uint8_t offset) const;
  uint32_t read32(uint8_t offset) const;
  void write8(uint8_t offset, uint8_t val);
  void write16(uint8_t offset, uint16_t val);
  void write32(uint8_t offset, uint32_t val);

public:
  pci_function(uint8_t bus, uint8_t device, uint8_t func);

  uint16_t get_vendor_id() const;
  uint16_t get_device_id() const;
  uint16_t get_command() const;
  uint16_t get_status() const;
  uint8_t get_revision_id() const;
  uint8_t get_prog_if() const;
  uint8_t get_subclass() const;
  uint8_t get_class_code() const;
  uint8_t get_cache_line_size() const;
  uint8_t get_latency_timer() const;
  uint8_t get_header_type() const;
  uint8_t get_bist() const;

  operator bool() const;
  bool is_multifunc() const;
  bool is_bridge() const;

  void set_command(uint16_t cmd);
  void set_bus_master(bool enable);
  void disable_int();

  void dump_address() const;
};

class pci_device;

class pci_bar {
  static const constexpr uint32_t BAR_IO_SPACE_FLAG = 0x1;
  static const constexpr uint32_t BAR_IO_SPACE_MASK = ~0x3;
  static const constexpr uint32_t BAR_MMIO_TYPE_MASK = 0x6;
  static const constexpr uint32_t BAR_MMIO_TYPE_32 = 0;
  static const constexpr uint32_t BAR_MMIO_TYPE_64 = 0x4;
  static const constexpr uint32_t BAR_MMIO_PREFETCHABLE_MASK = 0x8;
  static const constexpr uint32_t BAR_MMIO_MASK = ~0xf;

  uint64_t addr_;
  size_t size_;
  void *vaddr_;
  bool mmio_;
  bool is_64_;
  bool prefetchable_;

public:
  pci_bar(pci_device &dev, uint32_t bar_val, uint8_t idx);
  ~pci_bar();
  bool is_64() const;
  void map();
  uint8_t read8(size_t offset);
  uint16_t read16(size_t offset);
  uint32_t read32(size_t offset);
  void write8(size_t offset, uint8_t val);
  void write16(size_t offset, uint16_t val);
  void write32(size_t offset, uint32_t val);
};

class pci_device : public pci_function {
  static const constexpr uint8_t BAR0_ADDR = 0x10;
  static const constexpr uint8_t BAR1_ADDR = 0x14;
  static const constexpr uint8_t BAR2_ADDR = 0x18;
  static const constexpr uint8_t BAR3_ADDR = 0x1C;
  static const constexpr uint8_t BAR4_ADDR = 0x20;
  static const constexpr uint8_t BAR5_ADDR = 0x24;
  static constexpr uint8_t BAR_ADDR(uint8_t idx) { return BAR0_ADDR + idx * 4; }
  static const constexpr uint8_t CARDBUS_CIS_PTR_ADDR = 0x28;
  static const constexpr uint8_t SUBSYSTEM_VENDOR_ID_ADDR = 0x2C;
  static const constexpr uint8_t SUBSYSTEM_ID_ADDR = 0x2E;
  static const constexpr uint8_t CAPABILITIES_PTR_ADDR = 0x34;
  static const constexpr uint8_t INTERRUPT_LINE_ADDR = 0x3C;
  static const constexpr uint8_t INTERRUPT_PIN_ADDR = 0x3D;
  static const constexpr uint8_t MIN_GRANT_ADDR = 0x3E;
  static const constexpr uint8_t MAX_LATENCY_ADDR = 0x3F;

  static const constexpr uint8_t CAP_PM = 0x01;
  static const constexpr uint8_t CAP_AGP = 0x02;
  static const constexpr uint8_t CAP_VPD = 0x03;
  static const constexpr uint8_t CAP_SLOTID = 0x04;
  static const constexpr uint8_t CAP_MSI = 0x05;
  static const constexpr uint8_t CAP_CHSWP = 0x06;
  static const constexpr uint8_t CAP_PCIX = 0x07;
  static const constexpr uint8_t CAP_HT = 0x08;
  static const constexpr uint8_t CAP_VENDOR = 0x09;
  static const constexpr uint8_t CAP_DEBUG = 0x0a;
  static const constexpr uint8_t CAP_CRES = 0x0b;
  static const constexpr uint8_t CAP_HOTPLUG = 0x0c;
  static const constexpr uint8_t CAP_SUBVENDOR = 0x0d;
  static const constexpr uint8_t CAP_AGP8X = 0x0e;
  static const constexpr uint8_t CAP_SECDEV = 0x0f;
  static const constexpr uint8_t CAP_EXPRESS = 0x10;
  static const constexpr uint8_t CAP_MSIX = 0x11;
  static const constexpr uint8_t CAP_SATA = 0x12;
  static const constexpr uint8_t CAP_PCIAF = 0x13;

  static const constexpr uint8_t MSIX_CONTROL = 0x2;
  static const constexpr uint8_t MSIX_TABLE_OFFSET = 0x4;

  static const constexpr uint16_t MSIX_CONTROL_FUNCTION_MASK = 1 << 14;
  static const constexpr uint16_t MSIX_CONTROL_ENABLE = 1 << 15;
  static const constexpr uint16_t MSIX_CONTROL_TABLE_SIZE_MASK = 0x7ff;

  static const constexpr uint32_t MSIX_TABLE_OFFSET_BIR_MASK = 0x7;

  static const constexpr size_t MSIX_TABLE_ENTRY_SIZE = 16;
  static const constexpr size_t MSIX_TABLE_ENTRY_ADDR = 0;
  static const constexpr size_t MSIX_TABLE_ENTRY_DATA = 8;
  static const constexpr size_t MSIX_TABLE_ENTRY_CONTROL = 12;

  static const constexpr uint32_t MSIX_TABLE_ENTRY_CONTROL_MASK_BIT = 1;

  uint8_t find_capability(uint8_t capability) const;
  friend class pci_bar;
  uint32_t get_bar_raw(uint8_t idx) const;
  void set_bar_raw(uint8_t idx, uint32_t val);
  uint16_t msix_get_control();
  void msix_set_control(uint16_t control);

  std::array<boost::optional<pci_bar>, 6> bars_;
  int msix_bar_idx_;
  uint8_t msix_offset_;
  size_t msix_table_size_;
  uint32_t msix_table_offset_;
public:
  pci_device(uint8_t bus, uint8_t device, uint8_t func);

  bool msix_enabled() const;

  pci_bar &get_bar(uint8_t idx);
  bool msix_enable();
  void msix_mask_entry(size_t idx);
  void msix_unmask_entry(size_t idx);
  void set_msix_entry(size_t entry, uint8_t vector, uint8_t dest);
};

void pci_register_probe(std::function<bool(pci_device &)> probe);

void pci_load_drivers();
}
