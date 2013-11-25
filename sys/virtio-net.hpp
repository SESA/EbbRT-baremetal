#include <sys/virtio.hpp>

namespace ebbrt {
// class virtio_net_driver : public virtio_driver<virtio_net_driver> {

//   struct virtio_net_header {
//     static const constexpr uint8_t VIRTIO_NET_HDR_F_NEEDS_CSUM = 1;
//     static const constexpr uint8_t VIRTIO_NET_HDR_F_DATA_VALID = 2;
//     uint8_t flags;

//     static const constexpr uint8_t VIRTIO_NET_HDR_GSO_NONE = 0;
//     static const constexpr uint8_t VIRTIO_NET_HDR_GSO_TCPV4 = 1;
//     static const constexpr uint8_t VIRTIO_NET_HDR_GSO_UDP = 3;
//     static const constexpr uint8_t VIRTIO_NET_HDR_GSO_TCPV6 = 4;
//     static const constexpr uint8_t VIRTIO_NET_HDR_GSO_ECN = 0x80;
//     uint8_t gso_type;
//     uint16_t hdr_len;
//     uint16_t gso_size;
//     uint16_t csum_start;
//     uint16_t csum_offset;
//   };

//   struct virtio_net_header_mrg_rxbuf {
//     virtio_net_header hdr;
//     uint16_t num_buffers;
//   };

//   uint8_t mac_[6];
//   bool mergeable_bufs_;

//   size_t header_size();
// public:
//   virtio_net_driver(pci::device dev);
//   uint32_t get_driver_features();
// };
}
