#include <sys/virtio.hpp>

namespace ebbrt {
class virtio_net_driver : public virtio_driver<virtio_net_driver> {
  static const constexpr uint32_t VIRTIO_NET_F_CSUM = 0;
  static const constexpr uint32_t VIRTIO_NET_F_GUEST_CSUM = 1;
  static const constexpr uint32_t VIRTIO_NET_F_MAC = 5;
  static const constexpr uint32_t VIRTIO_NET_F_GSO = 6;
  static const constexpr uint32_t VIRTIO_NET_F_GUEST_TSO4 = 7;
  static const constexpr uint32_t VIRTIO_NET_F_GUEST_TSO6 = 8;
  static const constexpr uint32_t VIRTIO_NET_F_GUEST_ECN = 9;
  static const constexpr uint32_t VIRTIO_NET_F_GUEST_UFO = 10;
  static const constexpr uint32_t VIRTIO_NET_F_HOST_TSO4 = 11;
  static const constexpr uint32_t VIRTIO_NET_F_HOST_TSO6 = 12;
  static const constexpr uint32_t VIRTIO_NET_F_HOST_ECN = 13;
  static const constexpr uint32_t VIRTIO_NET_F_HOST_UFO = 14;
  static const constexpr uint32_t VIRTIO_NET_F_MRG_RXBUF = 15;
  static const constexpr uint32_t VIRTIO_NET_F_STATUS = 16;
  static const constexpr uint32_t VIRTIO_NET_F_CTRL_VQ = 17;
  static const constexpr uint32_t VIRTIO_NET_F_CTRL_RX = 18;
  static const constexpr uint32_t VIRTIO_NET_F_CTRL_VLAN = 19;
  static const constexpr uint32_t VIRTIO_NET_F_GUEST_ANNOUNCE = 21;

  struct virtio_net_header {
    static const constexpr uint8_t VIRTIO_NET_HDR_F_NEEDS_CSUM = 1;
    static const constexpr uint8_t VIRTIO_NET_HDR_F_DATA_VALID = 2;
    uint8_t flags;

    static const constexpr uint8_t VIRTIO_NET_HDR_GSO_NONE = 0;
    static const constexpr uint8_t VIRTIO_NET_HDR_GSO_TCPV4 = 1;
    static const constexpr uint8_t VIRTIO_NET_HDR_GSO_UDP = 3;
    static const constexpr uint8_t VIRTIO_NET_HDR_GSO_TCPV6 = 4;
    static const constexpr uint8_t VIRTIO_NET_HDR_GSO_ECN = 0x80;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
  };

  struct virtio_net_header_mrg_rxbuf {
    virtio_net_header hdr;
    uint16_t num_buffers;
  };

  uint8_t mac_[6];
  bool mergeable_bufs_;

  size_t header_size();
public:
  virtio_net_driver(pci::device dev);
  uint32_t get_driver_features();
};
}
