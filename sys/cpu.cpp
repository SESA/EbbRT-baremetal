#include <sys/cpu.hpp>

using namespace ebbrt;

thread_local cpu* ebbrt::my_cpu_tls;

boost::container::static_vector<cpu, MAX_NUM_CPUS> ebbrt::cpus;

char cpu::boot_interrupt_stack[PAGE_SIZE];
