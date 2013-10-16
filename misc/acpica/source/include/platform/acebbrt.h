#pragma once

#include <stdint.h>
#include "acgcc.h"

#define COMPILER_DEPENDENT_INT64 int64_t
#define COMPILER_DEPENDENT_UINT64 uint64_t

#define ACPI_MUTEX_TYPE ACPI_BINARY_SEMAPHORE

extern int AcpiOsAcquireGlobalLock(uint32_t *lock);
extern int AcpiOsReleaseGlobalLock(uint32_t *lock);

#define ACPI_ACQUIRE_GLOBAL_LOCK(GLptr, Acq)                                   \
  do {                                                                         \
    (Acq) = AcpiOsAcquireGlobalLock(&((GLptr)->GlobalLock));                   \
  } while (0)

#define ACPI_RELEASE_GLOBAL_LOCK(GLptr, Acq)                                   \
  do {                                                                         \
    (Acq) = AcpiOsReleaseGlobalLock(&((GLptr)->GlobalLock));                   \
  } while (0)

#define ACPI_FLUSH_CPU_CACHE()                                                 \
  do {                                                                         \
    asm volatile("wbinvd");                                                    \
  } while (0)

#define ACPI_USE_SYSTEM_CLIBRARY
#define ACPI_USE_STANDARD_HEADERS
#define ACPI_MACHINE_WIDTH 64
