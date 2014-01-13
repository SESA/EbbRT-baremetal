#include <cstring>

#include <lwip/dhcp.h>
#include <lwip/init.h>
#include <lwip/stats.h>
#include <lwip/sys.h>
#include <lwip/timers.h>
#include <netif/etharp.h>

#include <sys/arch/cc.h>
#include <sys/clock.hpp>
#include <sys/debug.hpp>
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
  if (idx == 0)
    netif_set_default(&netif_);

  netif_set_status_callback(&netif_, status_callback);
  dhcp_start(&netif_);
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

extern "C" uint32_t lwip_rand() {
  return rdtsc() % 0xFFFFFFFF;
}

u32_t sys_now() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(clock_time())
      .count();
}
