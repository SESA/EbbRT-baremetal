#include <sys/debug.hpp>
#include <sys/power.hpp>

extern "C" {
  #include "acpi.h"
}
namespace ebbrt {
void poweroff()
{
  auto status = AcpiInitializeSubsystem();
  if (ACPI_FAILURE(status)) {
    kprintf("AcpiInitializeSubsystem failed: %s\n", AcpiFormatException(status));
    kabort();
  }
  status = AcpiLoadTables();
  if (ACPI_FAILURE(status)) {
    kprintf("AcpiLoadTables failed: %s\n", AcpiFormatException(status));
    kabort();
  }
  status = AcpiEnterSleepStatePrep(ACPI_STATE_S5);
  if (ACPI_FAILURE(status)) {
    kprintf("AcpiEnterSleepStatePrep failed: %s\n", AcpiFormatException(status));
    kabort();
  }
  status = AcpiEnterSleepState(ACPI_STATE_S5);
  if (ACPI_FAILURE(status)) {
    kprintf("AcpiEnterSleepState failed: %s\n", AcpiFormatException(status));
    kabort();
  }

  kabort();
}
}
