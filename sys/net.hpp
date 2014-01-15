#pragma once

#include <vector>

#include <lwip/netif.h>

#include <sys/buffer.hpp>
#include <sys/main.hpp>
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
    friend class NetworkManager;

   public:
    Interface(EthernetDevice& ether_dev, size_t idx);
    void ReceivePacket(mutable_buffer buf, uint32_t len);
    void Send(const_buffer_list l);
    const std::array<char, 6>& MacAddress();
  };

  Interface& NewInterface(EthernetDevice& ether_dev);
  class Tcp_pcb {
    struct tcp_pcb* pcb_;
    std::function<void(Tcp_pcb)> accept_callback_;
    std::function<void()> connect_callback_;

    static err_t Accept_Handler(void *arg, struct tcp_pcb * newpcb, err_t err);
    static err_t Connect_Handler(void *arg, struct tcp_pcb * pcb, err_t err);
    Tcp_pcb(struct tcp_pcb *pcb);
   public:
    Tcp_pcb();
    ~Tcp_pcb();
    void Bind(uint16_t port);
    void Listen();
    void Accept(std::function<void(Tcp_pcb)> callback);
    void Connect(struct ip_addr *ipaddr, uint16_t port, std::function<void()> callback);
  };

 private:
  friend void ebbrt::kmain(ebbrt::MultibootInformation* mbi);
  void AcquireIPAddress();

  std::vector<Interface> interfaces_;
};
constexpr auto network_manager = EbbRef<NetworkManager>(network_manager_id);
}
