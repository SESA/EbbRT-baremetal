#include <cinttypes>

#include <sys/align.hpp>
#include <sys/debug.hpp>
#include <sys/explicitly_constructed.hpp>
#include <sys/io.hpp>
#include <sys/pci.hpp>
#include <sys/vmem.hpp>
#include <sys/vmem_allocator.hpp>

using namespace ebbrt;

namespace {

const constexpr uint32_t PCI_ADDRESS_PORT = 0xCF8;
const constexpr uint32_t PCI_DATA_PORT = 0xCFC;
const constexpr uint32_t PCI_ADDRESS_ENABLE = 0x80000000;
const constexpr uint8_t PCI_BUS_OFFSET = 16;
const constexpr uint8_t PCI_DEVICE_OFFSET = 11;
const constexpr uint8_t PCI_FUNC_OFFSET = 8;

void pci_set_addr(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
  out32(PCI_ADDRESS_PORT,
        PCI_ADDRESS_ENABLE | (bus << PCI_BUS_OFFSET) |
            (device << PCI_DEVICE_OFFSET) | (func << PCI_FUNC_OFFSET) | offset);
}

uint8_t pci_read_8(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
  pci_set_addr(bus, device, func, offset);
  return in8(PCI_DATA_PORT + (offset & 3));
}
uint16_t pci_read_16(uint8_t bus,
                     uint8_t device,
                     uint8_t func,
                     uint8_t offset) {
  pci_set_addr(bus, device, func, offset);
  return in16(PCI_DATA_PORT);
}

uint32_t pci_read_32(uint8_t bus,
                     uint8_t device,
                     uint8_t func,
                     uint8_t offset) {
  pci_set_addr(bus, device, func, offset);
  return in32(PCI_DATA_PORT);
}

void pci_write_16(uint8_t bus,
                  uint8_t device,
                  uint8_t func,
                  uint8_t offset,
                  uint16_t val) {
  pci_set_addr(bus, device, func, offset);
  out16(PCI_DATA_PORT, val);
}

void pci_write_32(uint8_t bus,
                  uint8_t device,
                  uint8_t func,
                  uint8_t offset,
                  uint32_t val) {
  pci_set_addr(bus, device, func, offset);
  out32(PCI_DATA_PORT, val);
}

explicitly_constructed<std::vector<pci_device> > devices;
explicitly_constructed<std::vector<std::function<bool(pci_device&)> > >
    driver_probes;

void enumerate_bus(uint8_t bus) {
  for (auto device = 0; device < 32; ++device) {
    auto dev = pci_function(bus, device, 0);
    if (dev)
      continue;

    auto nfuncs = dev.is_multifunc() ? 8 : 1;
    for (auto func = 0; func < nfuncs; ++func) {
      dev = pci_function(bus, device, func);
      if (dev)
        continue;

      if (dev.is_bridge()) {
        kabort("Secondary bus unsupported!\n");
      } else {
        devices->emplace_back(bus, device, func);
      }
    }
  }
}

void enumerate_all_buses() {
  auto bus = pci_function(0, 0, 0);
  if (!bus.is_multifunc()) {
    // single bus
    enumerate_bus(0);
  } else {
    // potentially multiple buses
    for (auto func = 0; func < 8; ++func) {
      bus = pci_function(0, 0, func);
      if (!bus)
        continue;
      enumerate_bus(func);
    }
  }
}
}

void ebbrt::pci_init() {
  devices.construct();
  driver_probes.construct();
  enumerate_all_buses();
}

void ebbrt::pci_register_probe(std::function<bool(pci_device&)> probe) {
  driver_probes->emplace_back(std::move(probe));
}

void ebbrt::pci_load_drivers() {
  for (auto& dev : *devices) {
    for (auto& probe : *driver_probes) {
      if (probe(dev))
        break;
    }
  }
}

pci_function::pci_function(uint8_t bus, uint8_t device, uint8_t func) : bus_ {
  bus
}
, device_ { device }
, func_ { func }
{}

uint8_t pci_function::read8(uint8_t offset) const {
  return pci_read_8(bus_, device_, func_, offset);
}

uint16_t pci_function::read16(uint8_t offset) const {
  return pci_read_16(bus_, device_, func_, offset);
}

uint32_t pci_function::read32(uint8_t offset) const {
  return pci_read_32(bus_, device_, func_, offset);
}

void pci_function::write16(uint8_t offset, uint16_t val) {
  return pci_write_16(bus_, device_, func_, offset, val);
}

void pci_function::write32(uint8_t offset, uint32_t val) {
  return pci_write_32(bus_, device_, func_, offset, val);
}

uint16_t pci_function::get_vendor_id() const { return read16(VENDOR_ID_ADDR); }
uint16_t pci_function::get_device_id() const { return read16(DEVICE_ID_ADDR); }
uint16_t pci_function::get_command() const { return read16(COMMAND_ADDR); }

uint8_t pci_function::get_header_type() const {
  return read8(HEADER_TYPE_ADDR) & ~HEADER_MULTIFUNC_MASK;
}

pci_function::operator bool() const { return get_vendor_id() == 0xffff; }

bool pci_function::is_multifunc() const {
  return read8(HEADER_TYPE_ADDR) & HEADER_MULTIFUNC_MASK;
}

bool pci_function::is_bridge() const {
  return get_header_type() == HEADER_TYPE_BRIDGE;
}

void pci_function::set_command(uint16_t cmd) { write16(COMMAND_ADDR, cmd); }

void pci_function::set_bus_master(bool enable) {
  auto cmd = get_command();
  if (enable) {
    cmd |= COMMAND_BUS_MASTER;
  } else {
    cmd &= ~COMMAND_BUS_MASTER;
  }
  set_command(cmd);
}

void pci_function::disable_int() {
  auto cmd = get_command();
  cmd |= COMMAND_INT_DISABLE;
  set_command(cmd);
}

void pci_function::dump_address() const {
  kprintf("%u:%u:%u\n", bus_, device_, func_);
}

pci_bar::pci_bar(pci_device& dev, uint32_t bar_val, uint8_t idx)
    : vaddr_(nullptr), is_64_(false), prefetchable_(false) {
  mmio_ = !(bar_val & BAR_IO_SPACE_FLAG);
  if (mmio_) {
    addr_ = bar_val & BAR_MMIO_MASK;
    is_64_ = (bar_val & BAR_MMIO_TYPE_MASK) == BAR_MMIO_TYPE_64;
    prefetchable_ = bar_val & BAR_MMIO_PREFETCHABLE_MASK;

    dev.set_bar_raw(idx, 0xffffffff);
    auto low_sz = dev.get_bar_raw(idx) & BAR_MMIO_MASK;
    dev.set_bar_raw(idx, bar_val);

    uint32_t high_sz = ~0;
    if (is_64_) {
      auto bar_val2 = dev.get_bar_raw(idx + 1);
      addr_ |= static_cast<uint64_t>(bar_val2) << 32;
      dev.set_bar_raw(idx + 1, 0xffffffff);
      high_sz = dev.get_bar_raw(idx + 1);
      dev.set_bar_raw(idx + 1, bar_val2);
    }
    uint64_t sz = static_cast<uint64_t>(high_sz) << 32 | low_sz;
    size_ = ~sz + 1;
  } else {
    addr_ = bar_val & BAR_IO_SPACE_MASK;
    dev.set_bar_raw(idx, 0xffffffff);
    auto sz = dev.get_bar_raw(idx) & BAR_IO_SPACE_MASK;
    dev.set_bar_raw(idx, bar_val);

    size_ = ~sz + 1;
  }

  kprintf("PCI: %u:%u:%u - BAR%u %#018" PRIx64 " - %#018" PRIx64 "\n",
          dev.bus_,
          dev.device_,
          dev.func_,
          idx,
          addr_,
          addr_ + size_);
}

pci_bar::~pci_bar() {
  kbugon(vaddr_ != nullptr, "pci_bar: Need to free mapped region\n");
}

bool pci_bar::is_64() const { return is_64_; }

void pci_bar::map() {
  if (!mmio_)
    return;

  auto npages = align_up(size_, PAGE_SIZE) >> PAGE_SHIFT;
  auto page = vmem_allocator->Alloc(npages);
  vaddr_ = reinterpret_cast<void*>(pfn_to_addr(page));
  kbugon(page == 0, "Failed to allocate virtual pages for mmio\n");
  map_memory(page, pfn_down(addr_), size_);
}

uint8_t pci_bar::read8(size_t offset) {
  if (mmio_) {
    auto addr = static_cast<void*>(static_cast<char*>(vaddr_) + offset);
    return *static_cast<volatile uint8_t*>(addr);
  } else {
    return in8(addr_ + offset);
  }
}

uint16_t pci_bar::read16(size_t offset) {
  if (mmio_) {
    auto addr = static_cast<void*>(static_cast<char*>(vaddr_) + offset);
    return *static_cast<volatile uint16_t*>(addr);
  } else {
    return in16(addr_ + offset);
  }
}

uint32_t pci_bar::read32(size_t offset) {
  if (mmio_) {
    auto addr = static_cast<void*>(static_cast<char*>(vaddr_) + offset);
    return *static_cast<volatile uint32_t*>(addr);
  } else {
    return in32(addr_ + offset);
  }
}

void pci_bar::write8(size_t offset, uint8_t val) {
  if (mmio_) {
    auto addr = static_cast<void*>(static_cast<char*>(vaddr_) + offset);
    *static_cast<volatile uint8_t*>(addr) = val;
  } else {
    out8(addr_ + offset, val);
  }
}

void pci_bar::write16(size_t offset, uint16_t val) {
  if (mmio_) {
    auto addr = static_cast<void*>(static_cast<char*>(vaddr_) + offset);
    *static_cast<volatile uint16_t*>(addr) = val;
  } else {
    out16(addr_ + offset, val);
  }
}

void pci_bar::write32(size_t offset, uint32_t val) {
  if (mmio_) {
    auto addr = static_cast<void*>(static_cast<char*>(vaddr_) + offset);
    *static_cast<volatile uint32_t*>(addr) = val;
  } else {
    out32(addr_ + offset, val);
  }
}

pci_device::pci_device(uint8_t bus, uint8_t device, uint8_t func)
    : pci_function {
  bus, device, func
}
, msix_bar_idx_(-1) {
  for (auto idx = 0; idx <= 6; ++idx) {
    auto bar = read32(BAR_ADDR(idx));
    if (bar == 0)
      continue;

    bars_[idx] = pci_bar(*this, bar, idx);

    if (bars_[idx]->is_64())
      idx++;
  }
}

uint8_t pci_device::find_capability(uint8_t capability) const {
  auto ptr = read8(CAPABILITIES_PTR_ADDR);
  while (ptr != 0) {
    auto cap = read8(ptr);
    if (cap == capability)
      return ptr;

    ptr = read8(ptr + 1);
  }

  return 0xFF;
}

uint32_t pci_device::get_bar_raw(uint8_t idx) const {
  return read32(BAR_ADDR(idx));
}

void pci_device::set_bar_raw(uint8_t idx, uint32_t val) {
  write32(BAR_ADDR(idx), val);
}

uint16_t pci_device::msix_get_control() {
  return read16(msix_offset_ + MSIX_CONTROL);
}

void pci_device::msix_set_control(uint16_t control) {
  write16(msix_offset_ + MSIX_CONTROL, control);
}

bool pci_device::msix_enabled() const { return msix_bar_idx_ != -1; }

pci_bar& pci_device::get_bar(uint8_t idx) {
  if (!bars_[idx])
    throw std::runtime_error("BAR does not exist\n");

  return *bars_[idx];
}

bool pci_device::msix_enable() {
  auto offset = find_capability(CAP_MSIX);
  if (offset == 0xFF)
    return false;

  msix_offset_ = offset;

  auto control = msix_get_control();
  msix_table_size_ = (control & MSIX_CONTROL_TABLE_SIZE_MASK) + 1;
  auto table_offset_val = read32(offset + MSIX_TABLE_OFFSET);
  msix_bar_idx_ = table_offset_val & MSIX_TABLE_OFFSET_BIR_MASK;
  msix_table_offset_ = table_offset_val & ~MSIX_TABLE_OFFSET_BIR_MASK;

  kprintf("MSIX - %u entries at BAR%u:%lx\n",
          msix_table_size_,
          msix_bar_idx_,
          msix_table_offset_);

  auto& msix_bar = get_bar(msix_bar_idx_);
  msix_bar.map();

  disable_int();

  auto ctrl = msix_get_control();
  ctrl |= MSIX_CONTROL_ENABLE;
  ctrl |= MSIX_CONTROL_FUNCTION_MASK;
  msix_set_control(ctrl);

  for (size_t i = 0; i < msix_table_size_; ++i) {
    msix_mask_entry(i);
  }

  ctrl &= ~MSIX_CONTROL_FUNCTION_MASK;
  msix_set_control(ctrl);

  return true;
}

void pci_device::msix_mask_entry(size_t idx) {
  if (!msix_enabled())
    return;

  if (idx >= msix_table_size_)
    return;

  auto& msix_bar = get_bar(msix_bar_idx_);
  auto offset = msix_table_offset_ + idx * MSIX_TABLE_ENTRY_SIZE +
                MSIX_TABLE_ENTRY_CONTROL;
  auto ctrl = msix_bar.read32(offset);
  ctrl |= MSIX_TABLE_ENTRY_CONTROL_MASK_BIT;
  msix_bar.write32(offset, ctrl);
}

void pci_device::msix_unmask_entry(size_t idx) {
  if (!msix_enabled())
    return;

  if (idx >= msix_table_size_)
    return;

  auto& msix_bar = get_bar(msix_bar_idx_);
  auto offset = msix_table_offset_ + idx * MSIX_TABLE_ENTRY_SIZE +
    MSIX_TABLE_ENTRY_CONTROL;
  auto ctrl = msix_bar.read32(offset);
  ctrl &= ~MSIX_TABLE_ENTRY_CONTROL_MASK_BIT;
  msix_bar.write32(offset, ctrl);
}

void pci_device::set_msix_entry(size_t entry, uint8_t vector, uint8_t dest) {
  auto& msix_bar = get_bar(msix_bar_idx_);
  auto offset = msix_table_offset_ + entry * MSIX_TABLE_ENTRY_SIZE;
  msix_bar.write32(offset + MSIX_TABLE_ENTRY_ADDR, 0xFEE00000);
  msix_bar.write32(offset + MSIX_TABLE_ENTRY_DATA, vector);
  msix_unmask_entry(entry);
}
