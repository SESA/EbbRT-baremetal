#include <malloc.h>

#include <atomic>
#include <cstring>

#include <sys/debug.hpp>
#include <sys/pci.hpp>
#include <sys/virtio-net.hpp>
#include <sys/vmem.hpp>

namespace ebbrt {
class virtio_net {
  struct Ring {
    uint16_t size;
    uint16_t free_head;
    uint16_t num_free;
    uint16_t last_used;
    virtio::QueueDescriptor *descs;
    virtio::Available *available;
    virtio::Used *used;
  };

  void InitRing(Ring &ring) {
    ring.size = virtio::queue_size(io_addr_);
    size_t bytes = virtio::qsz_bytes(ring.size);
    auto queue = memalign(4096, bytes);
    kassert(queue != nullptr);
    std::memset(queue, 0, bytes);
    auto vaddr = vmem::virt_to_phys(reinterpret_cast<uint64_t>(queue));
    virtio::queue_address(io_addr_, static_cast<uint32_t>(vaddr >> 12));
    ring.descs = static_cast<virtio::QueueDescriptor *>(queue);
    for (unsigned i = 0; i < ring.size; ++i) {
      ring.descs[i].next = i + 1;
    }
    ring.available =
        reinterpret_cast<virtio::Available *>(&ring.descs[ring.size]);
    ring.used = reinterpret_cast<virtio::Used *>(
        (reinterpret_cast<uintptr_t>(&ring.available->ring[ring.size]) + 4095) &
        ~4095);
    ring.free_head = 0;
    ring.num_free = ring.size;
    ring.last_used = 0;
  }
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

  static const constexpr size_t VIRTIO_HEADER_LEN = 10;

  pci::device dev_;
  // Ring receive_;
  Ring send_;
  uint16_t io_addr_;
  char mac_addr_[6];
  char empty_header_[VIRTIO_HEADER_LEN];

public:
  virtio_net(pci::device dev) : dev_{ std::move(dev) } {
    // auto ptr = dev_.find_capability(pci::device::CAP_MSIX);
    // kassert(ptr != 0);
    // dev.enable_msix(ptr);
    // auto bar = dev_.msix_table_BIR(ptr);
    // auto msix_addr = dev_.get_bar(bar);
    // auto offset = dev_.msix_table_offset(ptr);
    // kprintf("%x\n", msix_addr);
    // msix_addr += offset;
    // auto msix_table =
    //     reinterpret_cast<volatile pci::device::MSIXTableEntry *>(msix_addr);
    memset(empty_header_, 0, VIRTIO_HEADER_LEN);
    io_addr_ = static_cast<uint16_t>(dev_.get_bar(0) & ~0x3);
    dev_.set_bus_master(true);
    virtio::reset(io_addr_);
    virtio::acknowledge(io_addr_);
    virtio::driver(io_addr_);

    auto features = virtio::device_features(io_addr_);
    kassert(features & (1 << VIRTIO_NET_F_MAC));

    auto supported_features = 1 << VIRTIO_NET_F_MAC;
    virtio::guest_features(io_addr_, supported_features);
    uint16_t addr = io_addr_ + 20;
    for (unsigned i = 0; i < 6; ++i) {
      mac_addr_[i] = in8(addr + i);
    }

    virtio::queue_select(io_addr_, 1);
    // virtio::queue_vector(io_addr_, 1);
    InitRing(send_);

    std::atomic_thread_fence(std::memory_order_release);

    virtio::driver_ok(io_addr_);
  }

  void send() {
    // TODO: handle this by queuing the buffer to be sent out later
    kassert(send_.num_free > 2);
    send_.num_free -= 2;
    uint16_t head = send_.free_head;

    uint16_t index = head;
    send_.descs[index].address =
        vmem::virt_to_phys(reinterpret_cast<uint64_t>(empty_header_));
    send_.descs[index].length = VIRTIO_HEADER_LEN;
    send_.descs[index].flags.next = true;

    index = send_.descs[index].next;

    char data[] = "DEADBEEFDEADBEEF";
    send_.descs[index].address =
        vmem::virt_to_phys(reinterpret_cast<uint64_t>(data));
    send_.descs[index].length = strlen(data);
    send_.descs[index].flags.next = false;

    send_.free_head = send_.descs[index].next;

    auto avail = send_.available->index % send_.size;
    send_.available->ring[avail] = head;

    std::atomic_thread_fence(std::memory_order_release);

    send_.available->index++;

    std::atomic_thread_fence(std::memory_order_release);

    virtio::queue_notify(io_addr_, 1);
  }
};

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

  auto vnet = new virtio_net(std::move(dev));
  vnet->send();
  return true;
}

PCI_REGISTER_DRIVER(virtio_net_probe)
}
