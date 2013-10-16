#include <mutex>

#include <sys/align.hpp>
#include <sys/debug.hpp>
#include <sys/io.hpp>
#include <sys/pci.hpp>
#include <sys/pmem.hpp>
#include <sys/priority.hpp>
#include <sys/semaphore.hpp>
#include <sys/vmem.hpp>

extern "C" {
#include "acpi.h"
}

using namespace ebbrt;

ACPI_STATUS AcpiOsInitialize() { return AE_OK; }

ACPI_STATUS AcpiOsTerminate() {
  UNIMPLEMENTED();
  return AE_OK;
}

ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer() {
  ACPI_SIZE rsdp;
  if (ACPI_FAILURE(AcpiFindRootPointer(&rsdp))) {
    kprintf("Failed to find root pointer\n");
    kabort();
  }
  return rsdp;
}

ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *Initval,
                                     ACPI_STRING *NewVal) {
  *NewVal = nullptr;
  return AE_OK;
}

ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER *ExistingTable,
                                ACPI_TABLE_HEADER **NewTable) {
  *NewTable = nullptr;
  return AE_OK;
}

ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER *ExistingTable,
                                        ACPI_PHYSICAL_ADDRESS *NewAddress,
                                        UINT32 *NewTableLength) {
  *NewAddress = 0;
  *NewTableLength = 0;
  return AE_OK;
}

ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK *OutHandle) {
  *OutHandle = new std::mutex();
  // UNIMPLEMENTED();
  return AE_OK;
}

void AcpiOsDeleteLock(ACPI_SPINLOCK Handle) { UNIMPLEMENTED(); }

ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK Handle) {
  auto mut = static_cast<std::mutex *>(Handle);
  mut->lock();
  return 0;
}

void AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS Flags) {
  auto mut = static_cast<std::mutex *>(Handle);
  mut->unlock();
}

ACPI_STATUS
AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits,
                      ACPI_SEMAPHORE *OutHandle) {
  *OutHandle = new semaphore(InitialUnits);
  return AE_OK;
}

ACPI_STATUS
AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle) {
  UNIMPLEMENTED();
  return AE_OK;
}

ACPI_STATUS
AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units, UINT16 Timeout) {
  if (Handle == nullptr) {
    return AE_BAD_PARAMETER;
  }
  auto sem = static_cast<semaphore *>(Handle);
  switch (Timeout) {
  case ACPI_DO_NOT_WAIT:
    return sem->trywait(Units) ? AE_OK : AE_TIME;
  case ACPI_WAIT_FOREVER:
    sem->wait(Units);
    return AE_OK;
  default:
    UNIMPLEMENTED();
  }
}

ACPI_STATUS
AcpiOsSignalSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units) {
  if (Handle == nullptr) {
    return AE_BAD_PARAMETER;
  }
  auto sem = static_cast<semaphore *>(Handle);
  sem->signal(Units);
  return AE_OK;
}

void *AcpiOsAllocate(ACPI_SIZE Size) { return malloc(Size); }

void AcpiOsFree(void *Memory) { free(Memory); }

void *AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS Where, ACPI_SIZE Length) {
  uint64_t paddr = align_down(Where, pmem::page_size);
  auto diff = Where - paddr;
  size_t len = align_up(Length + diff, pmem::page_size);
  auto ret = vmem::allocate_page(len);
  if (ret == 0) {
    kprintf("Failed to allocate virtual pages for ACPI\n");
    kabort();
  }
  vmem::map(ret, paddr, len);
  return reinterpret_cast<void *>(ret + diff);
}

void AcpiOsUnmapMemory(void *LogicalAddress, ACPI_SIZE Size) {
  auto logaddr = reinterpret_cast<uint64_t>(LogicalAddress);
  uint64_t vaddr = align_down(logaddr, pmem::page_size);
  auto diff = logaddr - vaddr;
  uint64_t len = align_up(Size + diff, pmem::page_size);
  vmem::free_page_range(vaddr, len);
}

ACPI_STATUS
AcpiOsGetPhysicalAddress(void *LogicalAddress,
                         ACPI_PHYSICAL_ADDRESS *PhysicalAddress) {
  UNIMPLEMENTED();
  return AE_OK;
}

ACPI_STATUS
AcpiOsInstallInterruptHandler(UINT32 InterruptNumber,
                              ACPI_OSD_HANDLER ServiceRoutine, void *Context) {
  kprintf("%s: Unimplemented, but continuing\n", __PRETTY_FUNCTION__);
  return AE_OK;
}

ACPI_STATUS
AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber,
                             ACPI_OSD_HANDLER ServiceRoutine) {
  UNIMPLEMENTED();
  return AE_OK;
}

ACPI_THREAD_ID
AcpiOsGetThreadId(void) { return 1; }

ACPI_STATUS
AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function,
              void *Context) {
  UNIMPLEMENTED();
  return AE_OK;
}

void AcpiOsWaitEventsComplete(void) { UNIMPLEMENTED(); }

void AcpiOsSleep(UINT64 Milliseconds) { UNIMPLEMENTED(); }

void AcpiOsStall(UINT32 Microseconds) { UNIMPLEMENTED(); }

ACPI_STATUS
AcpiOsReadPort(ACPI_IO_ADDRESS Address, UINT32 *Value, UINT32 Width) {
  switch (Width) {
  case 8:
    *Value = in8(Address);
    break;
  case 16:
    *Value = in16(Address);
    break;
  case 32:
    *Value = in32(Address);
    break;
  default:
    return AE_BAD_PARAMETER;
  }
  return AE_OK;
}

ACPI_STATUS
AcpiOsWritePort(ACPI_IO_ADDRESS Address, UINT32 Value, UINT32 Width) {
  switch (Width) {
  case 8:
    out8(Address, Value);
    break;
  case 16:
    out16(Address, Value);
    break;
  case 32:
    out32(Address, Value);
    break;
  default:
    return AE_BAD_PARAMETER;
  }
  return AE_OK;
}

ACPI_STATUS
AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 *Value, UINT32 Width) {
  UNIMPLEMENTED();
  return AE_OK;
}

ACPI_STATUS
AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 Value, UINT32 Width) {
  UNIMPLEMENTED();
  return AE_OK;
}

ACPI_STATUS
AcpiOsReadPciConfiguration(ACPI_PCI_ID *PciId, UINT32 Reg, UINT64 *Value,
                           UINT32 Width) {
  switch (Width) {
  case 8:
    *(uint8_t *)Value =
        pci::read_config8(PciId->Bus, PciId->Device, PciId->Function, Reg);
    break;
  case 16:
    *(uint16_t *)Value =
        pci::read_config16(PciId->Bus, PciId->Device, PciId->Function, Reg);
    break;
  case 32:
    *(uint32_t *)Value =
        pci::read_config32(PciId->Bus, PciId->Device, PciId->Function, Reg);
    break;
  }
  return AE_OK;
}

ACPI_STATUS
AcpiOsWritePciConfiguration(ACPI_PCI_ID *PciId, UINT32 Reg, UINT64 Value,
                            UINT32 Width) {
  UNIMPLEMENTED();
  return AE_OK;
}

BOOLEAN
AcpiOsReadable(void *Pointer, ACPI_SIZE Length) {
  UNIMPLEMENTED();
  return false;
}

BOOLEAN
AcpiOsWritable(void *Pointer, ACPI_SIZE Length) {
  UNIMPLEMENTED();
  return false;
}

UINT64
AcpiOsGetTimer(void) {
  UNIMPLEMENTED();
  return 0;
}

ACPI_STATUS
AcpiOsSignal(UINT32 Function, void *Info) {
  UNIMPLEMENTED();
  return AE_OK;
}

void ACPI_INTERNAL_VAR_XFACE AcpiOsPrintf(const char *Format, ...) {
  va_list ap;
  va_start(ap, Format);
  kvprintf(Format, ap);
}

void AcpiOsVprintf(const char *Format, va_list Args) { kvprintf(Format, Args); }

void AcpiOsRedirectOutput(void *Destination) { UNIMPLEMENTED(); }

ACPI_STATUS
AcpiOsGetLine(char *Buffer, UINT32 BufferLength, UINT32 *BytesRead) {
  UNIMPLEMENTED();
  return AE_OK;
}

ACPI_STATUS
AcpiOsGetTableByName(char *Signature, UINT32 Instance,
                     ACPI_TABLE_HEADER **Table,
                     ACPI_PHYSICAL_ADDRESS *Address) {
  UNIMPLEMENTED();
  return AE_OK;
}

ACPI_STATUS
AcpiOsGetTableByIndex(UINT32 Index, ACPI_TABLE_HEADER **Table, UINT32 *Instance,
                      ACPI_PHYSICAL_ADDRESS *Address) {
  UNIMPLEMENTED();
  return AE_OK;
}

ACPI_STATUS
AcpiOsGetTableByAddress(ACPI_PHYSICAL_ADDRESS Address,
                        ACPI_TABLE_HEADER **Table) {
  UNIMPLEMENTED();
  return AE_OK;
}

void *AcpiOsOpenDirectory(char *Pathname, char *WildcardSpec,
                          char RequestedFileType) {
  UNIMPLEMENTED();
  return nullptr;
}

char *AcpiOsGetNextFilename(void *DirHandle) {
  UNIMPLEMENTED();
  return nullptr;
}

void AcpiOsCloseDirectory(void *DirHandle) { UNIMPLEMENTED(); }

constexpr uint32_t GL_ACQUIRED = ~0;
constexpr uint32_t GL_BUSY = 0;
constexpr uint32_t GL_BIT_PENDING = 1;
constexpr uint32_t GL_BIT_OWNED = 2;
constexpr uint32_t GL_BIT_MASK = (GL_BIT_PENDING | GL_BIT_OWNED);
int AcpiOsAcquireGlobalLock(uint32_t *lock) {
  uint32_t newval;
  uint32_t oldval;

  do {
    oldval = *lock;
    newval = ((oldval & ~GL_BIT_MASK) | GL_BIT_OWNED) |
             ((oldval >> 1) & GL_BIT_PENDING);
    __sync_lock_test_and_set(lock, newval);
  } while (*lock == oldval);
  return ((newval < GL_BIT_MASK) ? GL_ACQUIRED : GL_BUSY);
}

int AcpiOsReleaseGlobalLock(uint32_t *lock) {
  uint32_t newval;
  uint32_t oldval;
  do {
    oldval = *lock;
    newval = oldval & ~GL_BIT_MASK;
    __sync_lock_test_and_set(lock, newval);
  } while (*lock == oldval);
  return oldval & GL_BIT_PENDING;
}

namespace ebbrt {
namespace acpi {
ACPI_STATUS add_device(ACPI_HANDLE handle, uint32_t level, void *context,
                       void **ret_val) {
  ACPI_BUFFER name;

  name.Pointer = nullptr;
  name.Length = ACPI_ALLOCATE_BUFFER;

  auto status = AcpiGetName(handle, ACPI_FULL_PATHNAME, &name);
  if (ACPI_SUCCESS(status)) {
    kprintf("%s\n", name.Pointer);
  }

  ACPI_DEVICE_INFO *info;
  status = AcpiGetObjectInfo(handle, &info);
  if (ACPI_SUCCESS(status)) {
    if (info->Valid & ACPI_VALID_HID) {
      kprintf("\tHID: %s\n", info->HardwareId.String);
    }
    if (info->Valid & ACPI_VALID_CID) {
      for (size_t i = 0; i < info->CompatibleIdList.Count; ++i) {
        kprintf("\tCID: %s\n", info->CompatibleIdList.Ids[i].String);
      }
    }
    if (info->Valid & ACPI_VALID_ADR) {
      kprintf("\tADR: %.8X\n", info->Address);
    }
  }

  return AE_OK;
}

void init() {
  auto status = AcpiInitializeSubsystem();
  if (ACPI_FAILURE(status)) {
    kprintf("AcpiInitializeSubsystem failed: %s\n",
            AcpiFormatException(status));
    kabort();
  }
  status = AcpiInitializeTables(nullptr, 16, false);
  if (ACPI_FAILURE(status)) {
    kprintf("ACPI Initialization Failed\n");
    kabort();
  }
  status = AcpiLoadTables();
  if (ACPI_FAILURE(status)) {
    kprintf("AcpiLoadTables failed: %s\n", AcpiFormatException(status));
    kabort();
  }
  status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
  if (ACPI_FAILURE(status)) {
    kprintf("AcpiEnableSubsystem failed: %s\n", AcpiFormatException(status));
    kabort();
  }
  status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
  if (ACPI_FAILURE(status)) {
    kprintf("AcpiInitializeObjects failed: %s\n", AcpiFormatException(status));
    kabort();
  }

  // Use this to enumerate Acpi devices
  //AcpiGetDevices(nullptr, add_device, nullptr, nullptr);
}
}
}
