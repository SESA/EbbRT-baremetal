#pragma once

namespace ebbrt {
struct MultibootInformation;

extern "C" __attribute__((noreturn)) void
kmain(ebbrt::MultibootInformation *mbi);
}
