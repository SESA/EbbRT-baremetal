#pragma once

#include <sys/net.hpp>
#include <sys/virtio.hpp>

namespace ebbrt {
class virtio_net_driver : public virtio_driver<virtio_net_driver>,
                          public EthernetDevice {
  void fill_rx_ring();

  struct virtio_net_hdr {
    static const constexpr uint8_t VIRTIO_NET_HDR_F_NEEDS_CSUM = 1;
    uint8_t flags;
    static const constexpr uint8_t VIRTIO_NET_HDR_GSO_NONE = 0;
    static const constexpr uint8_t VIRTIO_NET_HDR_GSO_TCPV4 = 1;
    static const constexpr uint8_t VIRTIO_NET_HDR_GSO_UDP = 3;
    static const constexpr uint8_t VIRTIO_NET_HDR_GSO_TCPV6 = 4;
    static const constexpr uint8_t VIRTIO_NET_HDR_GSO_EVN = 0x80;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers;
  };
  virtio_net_hdr empty_header_;
  std::array<char, 6> mac_addr_;
  NetworkManager::Interface* itf_;

 public:
  static const constexpr uint16_t DEVICE_ID = 0x1000;

  static uint32_t get_driver_features();

  virtio_net_driver(pci_device& dev);
  void send(const_buffer_list list) override;
  const std::array<char, 6> &get_mac_address();
};
}
