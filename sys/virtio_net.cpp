#include <sys/debug.hpp>
#include <sys/event_manager.hpp>
#include <sys/virtio_net.hpp>

using namespace ebbrt;

namespace {
const constexpr int VIRTIO_NET_F_MAC = 5;
const constexpr int VIRTIO_NET_F_MRG_RXBUF = 15;
}

virtio_net_driver::virtio_net_driver(pci_device& dev)
    : virtio_driver<virtio_net_driver>(dev) {
  std::memset(static_cast<void*>(&empty_header_), 0, sizeof(empty_header_));

  for (int i = 0; i < 6; ++i) {
    mac_addr_[i] = device_config_read8(i);
  }

  kprintf("Mac Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
          mac_addr_[0],
          mac_addr_[1],
          mac_addr_[2],
          mac_addr_[3],
          mac_addr_[4],
          mac_addr_[5]);

  fill_rx_ring();

  auto rcv_vector = event_manager->AllocateVector([this]() {
    auto& rcv_queue = get_queue(0);
    rcv_queue.process_used_buffers([this](mutable_buffer_list list, size_t len) {
        kassert(list.size() == 1);
        list.front() += sizeof(virtio_net_hdr);
        itf_->ReceivePacket(std::move(list.front()), len - sizeof(virtio_net_hdr));
    });
    if (rcv_queue.get_num_free_descriptors() * 2 >= rcv_queue.size()) {
      fill_rx_ring();
    }
  });
  dev.set_msix_entry(0, rcv_vector, 0);

  auto send_vector = event_manager->AllocateVector([this]() {
    auto& send_queue = get_queue(1);
    send_queue.clean_used_buffers();
  });
  dev.set_msix_entry(1, send_vector, 0);

  add_device_status(CONFIG_DRIVER_OK);

  itf_ = &network_manager->NewInterface(*this);
}

void virtio_net_driver::fill_rx_ring() {
  auto& rcv_queue = get_queue(0);
  auto num_bufs = rcv_queue.get_num_free_descriptors();
  auto bufs = std::vector<mutable_buffer_list>(num_bufs);

  for (auto& buf_list : bufs) {
    buf_list.emplace_front(2048);
  }

  auto it = rcv_queue.add_writable_buffers(bufs.begin(), bufs.end());
  kassert(it == bufs.end());
}

uint32_t virtio_net_driver::get_driver_features() {
  return 1 << VIRTIO_NET_F_MAC | 1 << VIRTIO_NET_F_MRG_RXBUF;
}

void virtio_net_driver::send(const_buffer_list bufs) {
  bufs.emplace_front(static_cast<const void*>(&empty_header_),
                     sizeof(empty_header_));

  auto& send_queue = get_queue(1);
  kbugon(send_queue.get_num_free_descriptors() < bufs.size(),
         "Must queue a packet, no more room\n");

  send_queue.add_readable_buffer(std::move(bufs));
}

const std::array<char, 6> &virtio_net_driver::get_mac_address() { return mac_addr_; }
