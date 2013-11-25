#include <boost/utility.hpp>

#include <sys/cpu.hpp>
#include <sys/debug.hpp>
#include <sys/early_page_allocator.hpp>
#include <sys/numa.hpp>

using namespace ebbrt;

boost::container::static_vector<numa_node, MAX_NUMA_NODES> ebbrt::numa_nodes;

std::array<nid_t, MAX_LOCAL_APIC> ebbrt::apic_to_node_map;

std::array<nid_t, MAX_PXM_DOMAINS> ebbrt::pxm_to_node_map;

std::array<int32_t, MAX_NUMA_NODES> ebbrt::node_to_pxm_map;

void ebbrt::numa_init() {
  my_cpu_index = 0;
  cpus[my_cpu_index].nid = apic_to_node_map[cpus[my_cpu_index].apic_id];

  for (auto &numa_node : numa_nodes) {
    std::sort(numa_node.memblocks.begin(), numa_node.memblocks.end());
    for (auto &memblock : numa_node.memblocks) {
      early_set_nid_range(memblock.start, memblock.end, memblock.nid);
    }
    if (numa_node.memblocks.empty()) {
      numa_node.pfn_start = 0;
      numa_node.pfn_end = 0;
    } else {
      numa_node.pfn_start = numa_node.memblocks.begin()->start;
      numa_node.pfn_end = boost::prior(numa_node.memblocks.end())->end;
    }
  }
}
