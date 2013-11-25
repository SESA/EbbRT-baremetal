#pragma once

#include <boost/strong_typedef.hpp>
#include <boost/container/static_vector.hpp>

#include <sys/pfn.hpp>

namespace ebbrt {

const constexpr size_t MAX_NUMA_NODES = 256;
const constexpr size_t MAX_NUMA_MEMBLOCKS = 256;
const constexpr size_t MAX_PXM_DOMAINS = MAX_NUMA_NODES;
const constexpr size_t MAX_LOCAL_APIC = 256;

BOOST_STRONG_TYPEDEF(int, nid_t);
#define NO_NID                                                                 \
  nid_t { -1 }
#define ANY_NID nid_t(MAX_NUMA_NODES)

struct numa_memblock {
  numa_memblock(pfn_t start, pfn_t end, nid_t nid)
      : start{ start }, end{ end }, nid{ nid } {}
  pfn_t start;
  pfn_t end;
  nid_t nid;
};

inline bool operator<(const numa_memblock &lhs,
                      const numa_memblock &rhs) noexcept {
  return lhs.start < rhs.start;
}

struct numa_node {
  boost::container::static_vector<numa_memblock, MAX_NUMA_MEMBLOCKS> memblocks;
  pfn_t pfn_start;
  pfn_t pfn_end;
};

extern boost::container::static_vector<numa_node, MAX_NUMA_NODES> numa_nodes;

extern std::array<nid_t, MAX_LOCAL_APIC> apic_to_node_map;

extern std::array<nid_t, MAX_PXM_DOMAINS> pxm_to_node_map;

extern std::array<int32_t, MAX_NUMA_NODES> node_to_pxm_map;

void numa_init();
}
