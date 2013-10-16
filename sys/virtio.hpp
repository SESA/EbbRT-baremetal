#pragma once

#include <cstdint>
#include <sys/io.hpp>

namespace ebbrt {
template <class derived> class virtio_driver {
  pci::device dev_;
  uint16_t io_addr_;
  bool indirect_;
  bool event_idx_;

  static const constexpr uint8_t HOST_FEATURES = 0;
  static const constexpr uint8_t GUEST_FEATURES = 4;
  static const constexpr uint8_t QUEUE_ADDRESS = 8;
  static const constexpr uint8_t QUEUE_SIZE = 12;
  static const constexpr uint8_t QUEUE_SELECT = 14;
  static const constexpr uint8_t QUEUE_NOTIFY = 16;
  static const constexpr uint8_t DEVICE_STATUS = 18;
  static const constexpr uint8_t ISR_STATUS = 19;
  static const constexpr uint8_t MSI_CONFIG_VECTOR = 20;
  static const constexpr uint8_t MSI_QUEUE_VECTOR = 22;

  static const constexpr uint8_t VIRTIO_CONFIG_ACKNOWLEDGE = 1;
  static const constexpr uint8_t VIRTIO_CONFIG_DRIVER = 2;
  static const constexpr uint8_t VIRTIO_CONFIG_DRIVER_OK = 4;
  static const constexpr uint8_t VIRTIO_CONFIG_FAILED = 128;

  static const constexpr uint32_t VIRTIO_F_NOTIFY_ON_EMPTY = 24;
  static const constexpr uint32_t VIRTIO_F_RING_INDIRECT_DESC = 28;
  static const constexpr uint32_t VIRTIO_F_RING_EVENT_IDX = 29;

protected:
  explicit virtio_driver(pci::device dev) : dev_{ std::move(dev) } {
    io_addr_ = static_cast<uint16_t>(dev_.get_bar(0) & ~0x3);
    dev_.set_bus_master(true);
    // dev_.enable_msix();

    reset();
    add_device_status(VIRTIO_CONFIG_ACKNOWLEDGE | VIRTIO_CONFIG_DRIVER);
    setup_features();
  }

  void setup_features() {
    uint32_t device_features = get_device_features();
    uint32_t driver_features =
        static_cast<derived *>(this)->get_driver_features();

    uint32_t subset = device_features & driver_features;

    indirect_ = (subset >> VIRTIO_F_RING_INDIRECT_DESC) & 1;
    event_idx_ = (subset >> VIRTIO_F_RING_EVENT_IDX) & 1;
    set_guest_features(subset);
  }

  void reset() { set_device_status(0); }

  uint32_t get_driver_features() {
    return ((1 << VIRTIO_F_RING_INDIRECT_DESC) | 1 << VIRTIO_F_RING_EVENT_IDX);
  }

  size_t device_specific_config_offset() {
    return (dev_.is_msix_enabled()) ? 24 : 20;
  }

  uint32_t get_device_features() { return config_read32(HOST_FEATURES); }
  void set_guest_features(uint32_t features) {
    config_write32(GUEST_FEATURES, features);
  }
  uint32_t get_guest_features() { return config_read32(GUEST_FEATURES); }

  bool get_guest_feature_bit(uint32_t bit) {
    return get_guest_features() & (1 << bit);
  }
  uint8_t get_device_status() { return config_read8(DEVICE_STATUS); }



  void set_device_status(uint8_t status) {
    config_write8(DEVICE_STATUS, status);
  }

  void add_device_status(uint8_t status) {
    set_device_status(get_device_status() | status);
  }

  uint8_t config_read8(uint32_t offset) { return in8(io_addr_ + offset); }
  uint16_t config_read16(uint32_t offset) { return in16(io_addr_ + offset); }
  uint32_t config_read32(uint32_t offset) { return in32(io_addr_ + offset); }
  void config_write8(uint32_t offset, uint8_t val) {
    out8(io_addr_ + offset, val);
  }
  void config_write16(uint32_t offset, uint16_t val) {
    out16(io_addr_ + offset, val);
  }
  void config_write32(uint32_t offset, uint32_t val) {
    out32(io_addr_ + offset, val);
  }
};
}
