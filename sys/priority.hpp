#pragma once

#define FREEPAGE_INIT_PRIORITY 103
#define VIRT_FREEPAGE_INIT_PRIORITY 104
#define CPUS_INIT_PRIORITY 105

#define EVENT_PRIORITY 150

#define PCI_INIT_PRIORITY 151
#define PCI_DRIVER_PRIORITY 152
// #define PHYSMEM_INIT_PRIORITY 105
// #define VIRTMEM_INIT_PRIORITY 106
// #define IDT_INIT_PRIORITY 107
// #define CPU_INIT_PRIORITY 108
// #define APIC_INIT_PRIORITY 109
// #define ACPI_INIT_PRIORITY 110
// #define PCI_INIT_PRIORITY 111
// Everything with a higher value is reserved for EbbRT use
#define EBBRT_PRIORITY 200
