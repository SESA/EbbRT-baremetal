#pragma once

#include <vector>

#include <lwip/netif.h>

#include <sys/buffer.hpp>
#include <sys/trans.hpp>

namespace ebbrt {
class EthernetDevice {
 public:
  virtual void send(const_buffer_list l) = 0;
  virtual const std::array<char, 6>& get_mac_address() = 0;
  virtual ~EthernetDevice() {}
};

class NetworkManager {
 public:
  static void Init();
  static NetworkManager& HandleFault(EbbId id);

  class Interface {
    EthernetDevice& ether_dev_;
    struct netif netif_;
    size_t idx_;

   public:
    Interface(EthernetDevice& ether_dev, size_t idx);
    void ReceivePacket(mutable_buffer buf, uint32_t len);
    void Send(const_buffer_list l);
    const std::array<char, 6>& MacAddress();
  };

  Interface& NewInterface(EthernetDevice& ether_dev);

 private:
  std::vector<Interface> interfaces_;
};
constexpr auto network_manager = EbbRef<NetworkManager> { network_manager_id }
;
}
