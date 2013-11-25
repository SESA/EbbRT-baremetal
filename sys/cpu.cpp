#include <sys/cpu.hpp>

using namespace ebbrt;

thread_local size_t ebbrt::my_cpu_index;

boost::container::static_vector<cpu, MAX_NUM_CPUS> ebbrt::cpus;
