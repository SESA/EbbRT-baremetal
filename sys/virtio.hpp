#pragma once

#include <atomic>
#include <cstdint>

#include <sys/align.hpp>
#include <sys/buffer.hpp>
#include <sys/debug.hpp>
#include <sys/fls.hpp>
#include <sys/page_allocator.hpp>
#include <sys/pci.hpp>
#include <sys/pfn.hpp>
#include <sys/virtio.hpp>

namespace ebbrt {

template <typename virt_type> class virtio_driver {
 public:
  static const constexpr uint16_t VIRTIO_VENDOR_ID = 0x1AF4;

  static const constexpr size_t DEVICE_FEATURES = 0;
  static const constexpr size_t GUEST_FEATURES = 4;
  static const constexpr size_t QUEUE_ADDRESS = 8;
  static const constexpr size_t QUEUE_SIZE = 12;
  static const constexpr size_t QUEUE_SELECT = 14;
  static const constexpr size_t QUEUE_NOTIFY = 16;
  static const constexpr size_t DEVICE_STATUS = 18;
  static const constexpr size_t QUEUE_VECTOR = 22;
  static const constexpr size_t DEVICE_CONFIGURATION = 24;

  static const constexpr uint8_t CONFIG_ACKNOWLEDGE = 1;
  static const constexpr uint8_t CONFIG_DRIVER = 2;
  static const constexpr uint8_t CONFIG_DRIVER_OK = 4;

  static const constexpr int QUEUE_ADDRESS_SHIFT = 12;

  static const constexpr int VIRTIO_RING_F_EVENT_IDX = 29;

  class vring {

    struct desc {
      static const constexpr uint16_t F_NEXT = 1;
      static const constexpr uint16_t F_WRITE = 2;
      static const constexpr uint16_t F_INDIRECT = 4;

      uint64_t addr;
      uint32_t len;
      uint16_t flags;
      uint16_t next;
    };

    struct avail {
      static const constexpr uint16_t F_NO_INTERRUPT = 1;

      std::atomic<uint16_t> flags;
      std::atomic<volatile uint16_t> idx;
      uint16_t ring[];
    };

    struct used_elem {
      uint32_t id;
      uint32_t len;
    };

    struct used {
      static const constexpr uint16_t F_NO_NOTIFY = 1;

      std::atomic<uint16_t> flags;
      std::atomic<volatile uint16_t> idx;
      used_elem ring[];
    };

    virtio_driver<virt_type>& driver_;
    size_t idx_;
    void* addr_;
    desc* desc_;
    avail* avail_;
    used* used_;
    std::atomic<volatile uint16_t>* avail_event_;
    std::atomic<volatile uint16_t>* used_event_;
    uint16_t qsize_;
    uint16_t free_head_;
    uint16_t free_count_;
    std::unordered_map<uint16_t, const_buffer_list> buf_references_;

   public:
    vring(virtio_driver<virt_type>& driver, uint16_t qsize, size_t idx)
        : driver_(driver),
          idx_(idx),
          qsize_(qsize),
          free_head_(0),
          free_count_(qsize_) {
      auto sz =
          align_up(sizeof(desc) * qsize + sizeof(uint16_t) * (3 + qsize),
                   4096) +
          align_up(sizeof(uint16_t) * 3 + sizeof(used_elem) * qsize, 4096);
      auto order = fls(sz - 1) - PAGE_SHIFT + 1;
      auto page = page_allocator->Alloc(order);
      kbugon(page == NO_PFN, "virtio: page allocation failed");
      addr_ = reinterpret_cast<void*>(pfn_to_addr(page));
      memset(addr_, 0, sz);

      desc_ = static_cast<desc*>(addr_);
      avail_ = static_cast<avail*>(static_cast<void*>(
          static_cast<char*>(addr_) + qsize_ * sizeof(desc)));
      auto avail_ring_end = static_cast<void*>(&avail_->ring[qsize_]);
      used_event_ =
          reinterpret_cast<std::atomic<volatile uint16_t>*>(avail_ring_end);
      auto avail_end = static_cast<char*>(avail_ring_end) + sizeof(uint16_t);
      auto next_addr = align_up(static_cast<void*>(avail_end), 4096);
      used_ = static_cast<used*>(next_addr);
      auto used_ring_end = static_cast<void*>(&used_->ring[qsize_]);
      avail_event_ =
          reinterpret_cast<std::atomic<volatile uint16_t>*>(used_ring_end);

      for (unsigned i = 0; i < qsize_; ++i)
        desc_[i].next = i + 1;

      desc_[qsize_ - 1].next = 0;
    }

    void* get_addr() { return addr_; }

    size_t get_num_free_descriptors() { return free_count_; }

    template <typename Iterator>
    Iterator add_writable_buffers(Iterator begin, Iterator end) {
      if (begin == end)
        return end;
      auto count = 0;
      auto orig_idx = avail_->idx.load(std::memory_order_relaxed);
      auto avail_idx = orig_idx;
      for (auto it = begin; it < end; ++it) {
        ++count;
        auto& buf_list = *it;
        if (buf_list.empty())
          continue;

        if (buf_list.size() > free_count_)
          return it;

        //allocate the free descriptors
        free_count_ -= buf_list.size();
        uint16_t last_desc = free_head_;
        uint16_t head = free_head_;
        for (auto& buf : buf_list) {
          //for each buffer in this list, write it to a descriptor
          void* addr;
          size_t size;
          std::tie(addr, size) = buf.release();
          auto& desc = desc_[free_head_];
          desc.addr = reinterpret_cast<uint64_t>(addr);
          desc.len = static_cast<uint32_t>(size);
          desc.flags |= desc::F_WRITE | desc::F_NEXT;
          last_desc = free_head_;
          free_head_ = desc.next;
        }
        //make sure the last descriptor is marked as such
        desc_[last_desc].flags &= ~desc::F_NEXT;

        //add this descriptor chain to the avail ring
        avail_->ring[avail_idx % qsize_] = head;
        ++avail_idx;
      }
      //notify the device of the descriptor chains we added

      //ensure that all our writes to the descriptors and available ring have
      //completed
      std::atomic_thread_fence(std::memory_order_release);

      //give the device ownership of the added descriptor chains
      //note this need not have any memory ordering due to the preceding fence
      avail_->idx.store(avail_idx, std::memory_order_relaxed);

      //ensure that the previous write is seen before we detect if we must
      //notify the device. This ordering is to guarantee that the following
      //loads won't be ordered before the fence.
      std::atomic_thread_fence(std::memory_order_seq_cst);

      auto event_idx = avail_event_->load(std::memory_order_relaxed);
      if ((uint16_t)(avail_idx - event_idx - 1) <
          (uint16_t)(avail_idx - orig_idx)) {
        kick();
      }

      return end;
    }

    void add_readable_buffer(const_buffer_list bufs) {
      kbugon(free_count_ < bufs.size());

      free_count_ -= bufs.size();
      uint16_t last_desc = free_head_;
      uint16_t head = free_head_;
      for (auto& buf : bufs) {
        auto addr = buf.addr();
        auto size = buf.size();
        auto& desc = desc_[free_head_];
        desc.addr = reinterpret_cast<uint64_t>(addr);
        desc.len = size;
        desc.flags |= desc::F_NEXT;
        last_desc = free_head_;
        free_head_ = desc.next;
      }
      desc_[last_desc].flags &= ~desc::F_NEXT;

      auto avail_idx = avail_->idx.load(std::memory_order_relaxed);
      auto orig_idx = avail_idx;
      avail_->ring[avail_idx % qsize_] = head;
      ++avail_idx;

      std::atomic_thread_fence(std::memory_order_release);

      avail_->idx.store(avail_idx, std::memory_order_relaxed);

      std::atomic_thread_fence(std::memory_order_seq_cst);

      auto event_idx = avail_event_->load(std::memory_order_relaxed);
      if ((uint16_t)(avail_idx - event_idx - 1) <
          (uint16_t)(avail_idx - orig_idx)) {
        kick();
      }

      buf_references_.emplace(head, std::move(bufs));
    }

    template <typename F> void process_used_buffers(F&& f) {
      //future interrupts on this queue are implicitly disabled by our use of
      //the event index. We only get an interrupt when the new index crosses the
      //event index.
      auto used_index = used_event_->load(std::memory_order_relaxed);
      while (1) {
        if (used_index == used_->idx.load(std::memory_order_relaxed)) {
          //Ok we have processed all used buffers, set the event index to
          //re-enable interrupts
          used_event_->store(used_index, std::memory_order_relaxed);

          //to avoid a race, we must double check after this barrier
          std::atomic_thread_fence(std::memory_order_seq_cst);

          if (used_index == used_->idx.load(std::memory_order_relaxed))
            return;

          //otherwise the device did give us another used descriptor chain and
          //we must process it before enabling interrupts again.
        }
        auto& elem = used_->ring[used_index];
        mutable_buffer_list list;
        desc* descriptor = &desc_[elem.id];
        list.emplace_front(reinterpret_cast<void*>(descriptor->addr),
                           descriptor->len);
        auto it = list.begin();
        auto len = 1;
        while (descriptor->flags & desc::F_NEXT) {
          ++len;
          descriptor = &desc_[descriptor->next];
          it = list.emplace_after(
              it, reinterpret_cast<void*>(descriptor->addr), descriptor->len);
        }
        f(std::move(list), elem.len);
        //add the descriptor chain to the free list
        descriptor->next = free_head_;
        free_head_ = elem.id;
        free_count_ += len;

        ++used_index;
      }
    }

    void clean_used_buffers() {
      //future interrupts on this queue are implicitly disabled by our use of
      //the event index. We only get an interrupt when the new index crosses the
      //event index.
      auto used_index = used_event_->load(std::memory_order_relaxed);
      while (1) {
        if (used_index == used_->idx.load(std::memory_order_relaxed)) {
          //Ok we have processed all used buffers, set the event index to
          //re-enable interrupts
          used_event_->store(used_index, std::memory_order_relaxed);

          //to avoid a race, we must double check after this barrier
          std::atomic_thread_fence(std::memory_order_seq_cst);

          if (used_index == used_->idx.load(std::memory_order_relaxed))
            return;

          //otherwise the device did give us another used descriptor chain and
          //we must process it before enabling interrupts again.
        }
        auto& elem = used_->ring[used_index];
        buf_references_.erase(elem.id);
        desc* descriptor = &desc_[elem.id];
        auto len = 1;
        while (descriptor->flags & desc::F_NEXT) {
          ++len;
          descriptor = &desc_[descriptor->next];
        }
        //add the descriptor chain to the free list
        descriptor->next = free_head_;
        free_head_ = elem.id;
        free_count_ += len;

        ++used_index;
      }
    }

    uint16_t size() const { return qsize_; }

    void kick() { driver_.kick(idx_); }
  };

 private:
  uint8_t config_read8(size_t offset) { return bar0_.read8(offset); }

  uint16_t config_read16(size_t offset) { return bar0_.read16(offset); }

  uint32_t config_read32(size_t offset) { return bar0_.read32(offset); }

  void config_write8(size_t offset, uint8_t value) {
    bar0_.write8(offset, value);
  }

  void config_write16(size_t offset, uint16_t value) {
    bar0_.write16(offset, value);
  }

  void config_write32(size_t offset, uint32_t value) {
    bar0_.write32(offset, value);
  }

  void reset() { set_device_status(0); }

  uint8_t get_device_status() { return config_read8(DEVICE_STATUS); }

  void set_device_status(uint8_t status) {
    config_write8(DEVICE_STATUS, status);
  }

  void select_queue(uint16_t queue) { config_write16(QUEUE_SELECT, queue); }

  uint16_t get_queue_size() { return config_read16(QUEUE_SIZE); }

  void set_queue_addr(void* addr) {
    auto addr_val = reinterpret_cast<uintptr_t>(addr);
    addr_val >>= QUEUE_ADDRESS_SHIFT;
    kassert(addr_val <= std::numeric_limits<uint32_t>::max());
    config_write32(QUEUE_ADDRESS, addr_val);
  }

  void set_queue_vector(uint16_t index) { config_write16(QUEUE_VECTOR, index); }

  void setup_virt_queues() {
    while (1) {
      auto idx = queues_.size();
      select_queue(idx);
      auto qsize = get_queue_size();
      if (qsize == 0)
        return;

      queues_.emplace_back(*this, qsize, idx);
      set_queue_addr(queues_.back().get_addr());
      set_queue_vector(idx);
    }
  }

  uint32_t get_device_features() { return config_read32(DEVICE_FEATURES); }

  void set_guest_features(uint32_t features) {
    config_write32(GUEST_FEATURES, features);
  }

  void setup_features() {
    auto device_features = get_device_features();
    auto driver_features = virt_type::get_driver_features();

    driver_features |= 1 << VIRTIO_RING_F_EVENT_IDX;

    auto subset = device_features & driver_features;

    kassert(subset & 1 << VIRTIO_RING_F_EVENT_IDX);

    set_guest_features(subset);
  }

  pci_device& dev_;
  pci_bar& bar0_;

  std::vector<vring> queues_;

 public:
  static bool probe(pci_device& dev) {
    if (dev.get_vendor_id() == VIRTIO_VENDOR_ID &&
        dev.get_device_id() == virt_type::DEVICE_ID) {
      dev.dump_address();
      new virt_type(dev);
      return true;
    }
    return false;
  }

  virtio_driver(pci_device& dev) : dev_(dev), bar0_(dev.get_bar(0)) {

    dev_.set_bus_master(true);
    auto msix = dev_.msix_enable();
    kbugon(!msix, "Virtio without msix is unsupported\n");

    reset();

    add_device_status(CONFIG_ACKNOWLEDGE | CONFIG_DRIVER);

    setup_virt_queues();

    setup_features();
  }

  vring& get_queue(size_t index) { return queues_[index]; }

  void add_device_status(uint8_t status) {
    auto s = get_device_status();
    s |= status;
    set_device_status(s);
  }

  void kick(size_t idx) { config_write16(QUEUE_NOTIFY, idx); }

  uint8_t device_config_read8(size_t idx) {
    return config_read8(DEVICE_CONFIGURATION + idx);
  }
};
}
