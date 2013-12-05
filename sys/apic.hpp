#pragma once

#include <cstdint>

namespace ebbrt {
void apic_init();
const constexpr uint8_t DELIVERY_FIXED = 0;
const constexpr uint8_t DELIVERY_SMI = 2;
const constexpr uint8_t DELIVERY_NMI = 4;
const constexpr uint8_t DELIVERY_INIT = 5;
const constexpr uint8_t DELIVERY_STARTUP = 6;

void apic_ipi(uint8_t apic_id, uint8_t vector, bool level = true,
              uint8_t delivery_mode = DELIVERY_FIXED);
uint32_t apic_get_id();
void disable_pic();
}
