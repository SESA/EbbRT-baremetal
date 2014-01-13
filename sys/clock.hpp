#pragma once

#include <chrono>

namespace ebbrt {
void clock_init();
void clock_ap_init();
std::chrono::nanoseconds clock_time();
}
