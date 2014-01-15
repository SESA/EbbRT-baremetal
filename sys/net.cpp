#include <cstring>

#include <lwip/dhcp.h>
#include <lwip/init.h>
#include <lwip/stats.h>
#include <lwip/sys.h>
#include <lwip/tcp.h>
#include <lwip/timers.h>
#include <netif/etharp.h>

#include <sys/arch/cc.h>
#include <sys/clock.hpp>
#include <sys/debug.hpp>
#include <sys/event_manager.hpp>
#include <sys/explicitly_constructed.hpp>
#include <sys/net.hpp>
#include <sys/timer.hpp>

using namespace ebbrt;

namespace { explicitly_constructed<NetworkManager> the_manager; }

void NetworkManager::Init() {
  the_manager.construct();
  lwip_init();
  timer->Start(std::chrono::milliseconds(10),
               []() {
    sys_check_timeouts();
  });
}

NetworkManager& NetworkManager::HandleFault(EbbId id) {
  kassert(id == network_manager_id);
  auto& ref = *the_manager;
  cache_ref(id, ref);
  return ref;
}

NetworkManager::Interface& NetworkManager::NewInterface(
    EthernetDevice& ether_dev) {
  interfaces_.emplace_back(ether_dev, interfaces_.size());
  return interfaces_[interfaces_.size() - 1];
}

namespace { EventManager::EventContext* context; }

void NetworkManager::AcquireIPAddress() {
  kbugon(interfaces_.size() == 0, "No network interfaces identified!\n");
  netif_set_default(&interfaces_[0].netif_);
  dhcp_start(&interfaces_[0].netif_);
  context = new EventManager::EventContext;
  event_manager->SaveContext(*context);
}

namespace {
err_t eth_output(struct netif* netif, struct pbuf* p) {
  auto itf = static_cast<NetworkManager::Interface*>(netif->state);

#if ETH_PAD_SIZE
  pbuf_header(p, -ETH_PAD_SIZE);
#endif

  const_buffer_list l;
  for (struct pbuf* q = p; q != nullptr; q = q->next) {
    if (q->next == nullptr) {
      l.emplace_front(q->payload,
                      q->len,
                      [ = ](const void*) {
        pbuf_free(p);
      });
    } else {
      l.emplace_front(q->payload,
                      q->len,
                      [](const void*) {
      });
    }
  }
  itf->Send(std::move(l));
  pbuf_ref(p);

#if ETH_PAD_SIZE
  pbuf_header(p, ETH_PAD_SIZE);
#endif

  LINK_STATS_INC(link.xmit);

  return ERR_OK;
}

err_t eth_init(struct netif* netif) {
  auto itf = static_cast<NetworkManager::Interface*>(netif->state);
  netif->hwaddr_len = 6;
  memcpy(netif->hwaddr, itf->MacAddress().data(), 6);
  netif->mtu = 1500;
  netif->name[0] = 'e';
  netif->name[1] = 'n';
  netif->output = etharp_output;
  netif->linkoutput = eth_output;
  netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
  return ERR_OK;
}

void status_callback(struct netif* netif) {
  kprintf("IP address: %" U16_F ".%" U16_F ".%" U16_F ".%" U16_F "\n",
          ip4_addr1_16(&netif->ip_addr),
          ip4_addr2_16(&netif->ip_addr),
          ip4_addr3_16(&netif->ip_addr),
          ip4_addr4_16(&netif->ip_addr));
  event_manager->ActivateContext(*context);
  delete context;
}
}

NetworkManager::Interface::Interface(EthernetDevice& ether_dev, size_t idx)
    : ether_dev_(ether_dev), idx_(idx) {
  if (netif_add(&netif_,
                nullptr,
                nullptr,
                nullptr,
                static_cast<void*>(this),
                eth_init,
                ethernet_input) == nullptr) {
    throw std::runtime_error("Failed to create network interface");
  }
  netif_set_status_callback(&netif_, status_callback);
}

const std::array<char, 6>& NetworkManager::Interface::MacAddress() {
  return ether_dev_.get_mac_address();
}

void NetworkManager::Interface::Send(const_buffer_list l) {
  ether_dev_.send(std::move(l));
}

void NetworkManager::Interface::ReceivePacket(mutable_buffer buf,
                                              uint32_t len) {
  //FIXME: We should avoid this copy
  auto p = pbuf_alloc(PBUF_LINK, len + ETH_PAD_SIZE, PBUF_POOL);

  kbugon(p == nullptr, "Failed to allocate pbuf");

  auto ptr = buf.addr();
  bool first = true;
  for (auto q = p; q != nullptr; q = q->next) {
    auto add = 0;
    if (first) {
      add = ETH_PAD_SIZE;
      first = false;
    }
    memcpy(static_cast<char*>(q->payload) + add, ptr, q->len - add);
    ptr = static_cast<void*>(static_cast<char*>(ptr) + q->len);
  }

  netif_.input(p, &netif_);
}

extern "C" void lwip_printf(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  kvprintf(fmt, ap);
  va_end(ap);
}

extern "C" void lwip_assert(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  kvprintf(fmt, ap);
  va_end(ap);
  kabort();
}

extern "C" uint32_t lwip_rand() { return rdtsc() % 0xFFFFFFFF; }

u32_t sys_now() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(clock_time())
      .count();
}

NetworkManager::Tcp_pcb::Tcp_pcb() {
  pcb_ = tcp_new();
  if (pcb_ == nullptr) {
    throw std::bad_alloc();
  }
  tcp_arg(pcb_, static_cast<void*>(this));
}

NetworkManager::Tcp_pcb::Tcp_pcb(struct tcp_pcb* pcb) : pcb_(pcb) {
  tcp_arg(pcb_, static_cast<void*>(this));
}

NetworkManager::Tcp_pcb::~Tcp_pcb() {
  if (pcb_ != nullptr)
    tcp_abort(pcb_);
}

void NetworkManager::Tcp_pcb::Bind(uint16_t port) {
  auto ret = tcp_bind(pcb_, IP_ADDR_ANY, port);
  if (ret != ERR_OK) {
    throw std::runtime_error("Bind failed\n");
  }
}

void NetworkManager::Tcp_pcb::Listen() {
  auto pcb = tcp_listen(pcb_);
  if (pcb == NULL) {
    throw std::bad_alloc();
  }
  pcb_ = pcb;
}

void NetworkManager::Tcp_pcb::Accept(std::function<void(Tcp_pcb)> callback) {
  accept_callback_ = std::move(callback);
  tcp_accept(pcb_, Accept_Handler);
}

void NetworkManager::Tcp_pcb::Connect(struct ip_addr* ip,
                                      uint16_t port,
                                      std::function<void()> callback) {
  connect_callback_ = std::move(callback);
  auto err = tcp_connect(pcb_, ip, port, Connect_Handler);
  if (err != ERR_OK) {
    throw std::bad_alloc();
  }
}

err_t NetworkManager::Tcp_pcb::Accept_Handler(void* arg,
                                              struct tcp_pcb* newpcb,
                                              err_t err) {
  kassert(err == ERR_OK);
  auto listening_pcb = static_cast<Tcp_pcb*>(arg);
  listening_pcb->accept_callback_(Tcp_pcb(newpcb));
  tcp_accepted(listening_pcb->pcb_);
  return ERR_OK;
}

err_t NetworkManager::Tcp_pcb::Connect_Handler(void* arg,
                                               struct tcp_pcb* pcb,
                                               err_t err) {
  kassert(err == ERR_OK);
  auto pcb_s = static_cast<Tcp_pcb*>(arg);
  kassert(pcb_s->pcb_ == pcb);
  pcb_s->connect_callback_();
  return ERR_OK;
}
