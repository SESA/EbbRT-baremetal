#include <cstdint>

#include <sys/debug.hpp>
#include <sys/io.hpp>
#include <sys/pci.hpp>
#include <sys/priority.hpp>

namespace ebbrt {
namespace pci {
constexpr uint8_t VENDOR_ID = 0;
constexpr uint8_t COMMAND = 0x4;
constexpr uint8_t SUBCLASS = 0xa;
constexpr uint8_t CLASS_CODE = 0xb;
constexpr uint8_t HEADER_TYPE = 0xe;
constexpr uint8_t BAR_OFFSET = 0x10;
constexpr uint8_t SECONDARY_BUS_NUMBER = 0x19;

constexpr uint8_t HEADER_TYPE_MASK = 0x7f;
constexpr uint8_t HEADER_MULTI_FUNC = 0x80;

constexpr uint8_t HEADER_TYPE_BRIDGE = 0x01;

constexpr uint8_t COMMAND_BUS_MASTER = 1 << 2;

constexpr uint16_t ADDRESS_PORT = 0xCF8;
constexpr uint16_t DATA_PORT = 0xCFC;

struct address {
  union {
    uint32_t raw;
    struct {
      uint32_t reserved0 : 2;
      uint32_t offset : 6;
      uint32_t fnum : 3;
      uint32_t devnum : 5;
      uint32_t busnum : 8;
      uint32_t reserved1 : 7;
      uint32_t enable : 1;
    };
  };
};
void set_address(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
  address addr;
  addr.raw = 0;
  addr.enable = 1;
  addr.busnum = bus;
  addr.devnum = device;
  addr.fnum = func;
  addr.offset = offset >> 2;
  out32(ADDRESS_PORT, addr.raw);
}

uint8_t read_config8(uint8_t bus, uint8_t device, uint8_t func,
                     uint8_t offset) {
  set_address(bus, device, func, offset);
  return in32(DATA_PORT) >> ((offset & 0x3) * 8);
}

uint16_t read_config16(uint8_t bus, uint8_t device, uint8_t func,
                       uint8_t offset) {
  set_address(bus, device, func, offset);
  return in32(DATA_PORT) >> ((offset & 0x3) * 8);
}

uint32_t read_config32(uint8_t bus, uint8_t device, uint8_t func,
                       uint8_t offset) {
  set_address(bus, device, func, offset);
  return in32(DATA_PORT);
}

void write_config16(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset,
                    uint16_t val) {
  set_address(bus, device, func, offset);
  auto read_val = in32(DATA_PORT);
  read_val =
      (read_val & ~(0xFFFF << ((offset & 3) * 8))) | val << ((offset & 3) * 8);
  out32(read_val, DATA_PORT);
}

void write_config32(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset,
                    uint32_t val) {
  set_address(bus, device, func, offset);
  out32(val, DATA_PORT);
}

std::vector<bool (*)(device)> pci_driver_probes
    __attribute__((init_priority(PCI_INIT_PRIORITY)));

void enumerate_bus(uint8_t bus) {
  for (uint8_t device = 0; device < 32; ++device) {
    if (read_config16(bus, device, 0, VENDOR_ID) == 0xffff) {
      continue;
    }

    auto multi_func =
        (read_config8(bus, device, 0, HEADER_TYPE) & HEADER_MULTI_FUNC);
    uint8_t num_funcs = multi_func ? 8 : 1;

    for (uint8_t func = 0; func < num_funcs; func++) {
      if (read_config16(bus, device, 0, VENDOR_ID) == 0xffff) {
        continue;
      }

      // found a device or bridge
      if ((read_config8(bus, device, func, HEADER_TYPE) & HEADER_TYPE_MASK) ==
          HEADER_TYPE_BRIDGE) {
        kprintf("Found Bridge\n");
        enumerate_bus(read_config8(bus, device, func, SECONDARY_BUS_NUMBER));
      } else {
        auto dev = pci::device{ bus, device, func };
        for (const auto &probe : pci_driver_probes) {
          if (probe(dev)) {
            break;
          }
        }
      }
    }
  }
}

void init() {
  if (!(read_config8(0, 0, 0, HEADER_TYPE) & HEADER_MULTI_FUNC)) {
    enumerate_bus(0);
  } else {
    for (uint8_t func = 0; func < 8; ++func) {
      if (read_config16(0, 0, func, VENDOR_ID) != 0xffff) {
        break;
      }
      enumerate_bus(func);
    }
  }
}

device::device(uint8_t bus, uint8_t device, uint8_t func)
    : bus_{ bus }, device_{ device }, func_{ func }, msix_enabled_{ false } {}

uint8_t device::read8(uint8_t offset) const {
  return read_config8(bus_, device_, func_, offset);
}
uint16_t device::read16(uint8_t offset) const {
  return read_config16(bus_, device_, func_, offset);
}
uint32_t device::read32(uint8_t offset) const {
  return read_config32(bus_, device_, func_, offset);
}
void device::write16(uint8_t offset, uint16_t val) {
  write_config16(bus_, device_, func_, offset, val);
}
void device::write32(uint8_t offset, uint32_t val) {
  write_config32(bus_, device_, func_, offset, val);
}

uint16_t device::get_vendor_id() const { return read16(VENDOR_ID_ADDR); }
uint16_t device::get_device_id() const { return read16(DEVICE_ID_ADDR); }
uint16_t device::get_command() const { return read16(COMMAND_ADDR); }
void device::set_command(uint16_t val) { write16(COMMAND_ADDR, val); }
uint8_t device::get_revision_id() const { return read8(REVISION_ID_ADDR); }
uint16_t device::get_subsystem_vendor_id() const {
  return read16(SUBSYSTEM_VENDOR_ID_ADDR);
}
uint16_t device::get_subsystem_id() const { return read16(SUBSYSTEM_ID_ADDR); }
void device::set_bus_master(bool master) {
  auto command = get_command();
  command &= ~COMMAND_BUS_MASTER;
  command |= master;
  set_command(command);
}
uint32_t device::get_bar(uint8_t bar) const {
  return read32(BAR_OFFSET + bar * 4);
}
uint8_t device::get_capabilities() const {
  return read8(CAPABILITIES_PTR_ADDR);
}
uint8_t device::find_capability(uint8_t id) const {
  uint8_t ptr = get_capabilities() & ~0x3;
  while (ptr) {
    uint8_t type = read8(ptr);
    if (type == id) {
      return ptr;
    }
    ptr = read8(ptr + 1) & ~0x3;
  }
  return 0;
}
void device::enable_msix(uint8_t ptr) {
  auto control = read16(ptr + 2);
  control |= 1 << 15;
  write16(control, ptr + 2);
  msix_enabled_ = true;
}

bool device::is_msix_enabled() const { return msix_enabled_; }

uint8_t device::msix_table_BIR(uint8_t ptr) const {
  return read32(ptr + 4) & 0x7;
}

uint32_t device::msix_table_offset(uint8_t ptr) const {
  return read32(ptr + 4) & ~0x7;
}
}
}
