#include <sys/debug.hpp>
#include <sys/pci.hpp>
#include <sys/virtio-net.hpp>

namespace ebbrt {
bool virtio_net_probe(pci::device dev) {
  auto vendor_id = dev.get_vendor_id();
  if (vendor_id != 0x1AF4) {
    return false;
  }
  auto device_id = dev.get_device_id();
  if (device_id < 0x1000 || device_id > 0x103F) {
    return false;
  }
  auto revision_id = dev.get_revision_id();
  if (revision_id != 0) {
    return false;
  }
  auto subsystem_id = dev.get_subsystem_id();
  if (subsystem_id != 1) {
    return false;
  }

  new virtio_net_driver(std::move(dev));
  return true;
}

PCI_REGISTER_DRIVER(virtio_net_probe)

virtio_net_driver::virtio_net_driver(pci::device dev)
    : virtio_driver<virtio_net_driver>(std::move(dev)) {
  if (!get_guest_feature_bit(VIRTIO_NET_F_MAC)) {
    kprintf("No MAC Address!\n");
    kabort();
  }

  auto offset = device_specific_config_offset();
  for (size_t i = 0; i < 6; ++i) {
    mac_[i] = config_read8(offset + i);
  }
  kprintf("%.2X:%.2X:%.2X:%.2X:%.2X:%.2X\n", (uint32_t)mac_[0],
          (uint32_t)mac_[1], (uint32_t)mac_[2], (uint32_t)mac_[3],
          (uint32_t)mac_[4], (uint32_t)mac_[5]);

  mergeable_bufs_ = get_guest_feature_bit(VIRTIO_NET_F_MRG_RXBUF);
  auto msix_table = get_msix_table();
  add_device_status(VIRTIO_CONFIG_DRIVER_OK);
}

uint32_t virtio_net_driver::get_driver_features() {
  uint32_t base = virtio_driver<virtio_net_driver>::get_driver_features();
  return (base | (1 << VIRTIO_NET_F_MAC) | (1 << VIRTIO_NET_F_MRG_RXBUF));
}

size_t virtio_net_driver::header_size() {
  return mergeable_bufs_ ? sizeof(virtio_net_header_mrg_rxbuf)
                         : sizeof(virtio_net_header);
}
}
