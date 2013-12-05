#pragma once

namespace ebbrt {
void tls_init();
void tls_smp_init();
void tls_ap_init(size_t size);
}
