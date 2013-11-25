#include <cinttypes>

#include <sys/acpi.hpp>
// #include <sys/align.hpp>
#include <sys/cpu.hpp>
#include <sys/debug.hpp>
// #include <sys/io.hpp>
#include <sys/numa.hpp>
// #include <sys/pci.hpp>
// #include <sys/pmem.hpp>
// #include <sys/priority.hpp>
// #include <sys/semaphore.hpp>
#include <sys/vmem.hpp>

extern "C" {
#include "acpi.h"
}

using namespace ebbrt;

namespace {
const constexpr size_t ACPI_MAX_INIT_TABLES = 32;
ACPI_TABLE_DESC initial_table_storage[ACPI_MAX_INIT_TABLES];

bool initialized = false;

void parse_madt(const ACPI_TABLE_MADT *madt) {
  auto len = madt->Header.Length;
  auto madt_addr = reinterpret_cast<uintptr_t>(madt);
  auto offset = sizeof(ACPI_TABLE_MADT);
  while (offset < len) {
    auto subtable =
        reinterpret_cast<const ACPI_SUBTABLE_HEADER *>(madt_addr + offset);
    switch (subtable->Type) {
    case ACPI_MADT_TYPE_LOCAL_APIC: {
      auto local_apic =
          reinterpret_cast<const ACPI_MADT_LOCAL_APIC *>(subtable);
      if (local_apic->LapicFlags & ACPI_MADT_ENABLED) {
        kprintf("Local APIC: ACPI ID: %u APIC ID: %u\n",
                local_apic->ProcessorId, local_apic->Id);
        cpus.emplace_back(local_apic->ProcessorId, local_apic->Id);
      }
      offset += sizeof(ACPI_MADT_LOCAL_APIC);
      break;
    }
    case ACPI_MADT_TYPE_IO_APIC: {
      auto io_apic = reinterpret_cast<const ACPI_MADT_IO_APIC *>(subtable);
      kprintf("IO APIC: ID: %u\n", io_apic->Id);
      offset += sizeof(ACPI_MADT_IO_APIC);
      break;
    }
    case ACPI_MADT_TYPE_INTERRUPT_OVERRIDE: {
      auto interrupt_override =
          reinterpret_cast<const ACPI_MADT_INTERRUPT_OVERRIDE *>(subtable);
      kprintf("Interrupt Override: %u -> %u\n", interrupt_override->SourceIrq,
              interrupt_override->GlobalIrq);
      offset += sizeof(ACPI_MADT_INTERRUPT_OVERRIDE);
      break;
    }
    case ACPI_MADT_TYPE_NMI_SOURCE:
      kprintf("NMI_SOURCE MADT entry found, unsupported... aborting!\n");
      kabort();
      break;
    case ACPI_MADT_TYPE_LOCAL_APIC_NMI: {
      auto local_apic_nmi =
          reinterpret_cast<const ACPI_MADT_LOCAL_APIC_NMI *>(subtable);
      if (local_apic_nmi->ProcessorId == 0xff) {
        kprintf("NMI on all processors");
      } else {
        kprintf("NMI on processor %u", local_apic_nmi->ProcessorId);
      }
      kprintf(": LINT%u\n", local_apic_nmi->Lint);
      offset += sizeof(ACPI_MADT_LOCAL_APIC_NMI);
      break;
    }
    case ACPI_MADT_TYPE_LOCAL_APIC_OVERRIDE:
      kprintf(
          "LOCAL_APIC_OVERRIDE MADT entry found, unsupported... aborting!\n");
      kabort();
      break;
    case ACPI_MADT_TYPE_IO_SAPIC:
      kprintf("IO_SAPIC MADT entry found, unsupported... aborting!\n");
      kabort();
      break;
    case ACPI_MADT_TYPE_LOCAL_SAPIC:
      kprintf("LOCAL_SAPIC MADT entry found, unsupported... aborting!\n");
      kabort();
      break;
    case ACPI_MADT_TYPE_INTERRUPT_SOURCE:
      kprintf("INTERRUPT_SOURCE MADT entry found, unsupported... aborting!\n");
      kabort();
      break;
    case ACPI_MADT_TYPE_LOCAL_X2APIC:
      kprintf("LOCAL_X2APIC MADT entry found, unsupported... aborting!\n");
      kabort();
      break;
    case ACPI_MADT_TYPE_LOCAL_X2APIC_NMI:
      kprintf("LOCAL_X2APIC_NMI MADT entry found, unsupported... aborting!\n");
      kabort();
      break;
    case ACPI_MADT_TYPE_GENERIC_INTERRUPT:
      kprintf("GENERIC_INTERRUPT MADT entry found, unsupported... aborting!\n");
      kabort();
      break;
    case ACPI_MADT_TYPE_GENERIC_DISTRIBUTOR:
      kprintf(
          "GENERIC_DISTRIBUTOR MADT entry found, unsupported... aborting!\n");
      kabort();
      break;
    default:
      kprintf("Unrecognized MADT Entry MADT entry found, unsupported... "
              "aborting!\n");
      kabort();
      break;
    }
  }
}

nid_t setup_node(size_t proximity_domain) {
  kbugon(proximity_domain >= MAX_PXM_DOMAINS,
         "Proximity domain exceeds MAX_PXM_DOMAINS");
  auto node = pxm_to_node_map[proximity_domain];
  if (node == NO_NID) {
    numa_nodes.emplace_back();
    node = nid_t(numa_nodes.size() - 1);
    pxm_to_node_map[proximity_domain] = node;
    node_to_pxm_map[node] = proximity_domain;
  }
  return node;
}

void parse_srat(const ACPI_TABLE_SRAT *srat) {
  auto len = srat->Header.Length;
  auto srat_addr = reinterpret_cast<uintptr_t>(srat);
  auto offset = sizeof(ACPI_TABLE_SRAT);
  while (offset < len) {
    auto subtable =
        reinterpret_cast<const ACPI_SUBTABLE_HEADER *>(srat_addr + offset);
    switch (subtable->Type) {
    case ACPI_SRAT_TYPE_CPU_AFFINITY: {
      auto cpu_affinity =
          reinterpret_cast<const ACPI_SRAT_CPU_AFFINITY *>(subtable);
      if (cpu_affinity->Flags & ACPI_SRAT_CPU_USE_AFFINITY) {
        auto prox_domain = cpu_affinity->ProximityDomainLo |
                           cpu_affinity->ProximityDomainHi[0] << 8 |
                           cpu_affinity->ProximityDomainHi[1] << 16 |
                           cpu_affinity->ProximityDomainHi[2] << 24;
        kprintf("SRAT CPU affinity: %u -> %u\n", cpu_affinity->ApicId,
                prox_domain);
        auto node = setup_node(prox_domain);

        kbugon(cpu_affinity->ApicId >= MAX_LOCAL_APIC,
               "Apic Id exceeds MAX_LOCAL_APIC");
        apic_to_node_map[cpu_affinity->ApicId] = node;
      }
      offset += sizeof(ACPI_SRAT_CPU_AFFINITY);
      break;
    }
    case ACPI_SRAT_TYPE_MEMORY_AFFINITY: {
      auto mem_affinity =
          reinterpret_cast<const ACPI_SRAT_MEM_AFFINITY *>(srat_addr + offset);
      if (mem_affinity->Flags & ACPI_SRAT_MEM_ENABLED) {
        auto start = pfn_down(mem_affinity->BaseAddress);
        auto end = pfn_up(mem_affinity->BaseAddress + mem_affinity->Length);
        kprintf("SRAT Memory affinity: %#018" PRIx64 "-%#018" PRIx64 " -> %u\n",
                pfn_to_addr(start), pfn_to_addr(end) - 1,
                mem_affinity->ProximityDomain);

        auto node = setup_node(mem_affinity->ProximityDomain);

        numa_nodes[node].memblocks.emplace_back(start, end, node);
      }
      offset += sizeof(ACPI_SRAT_MEM_AFFINITY);
      break;
    }
    case ACPI_SRAT_TYPE_X2APIC_CPU_AFFINITY:
      kprintf(
          "X2APIC_CPU_AFFINITY SRAT entry found, unsupported... aborting!\n");
      kabort();
      break;
    }
  }
}
}

void ebbrt::acpi_boot_init() {
  std::fill(apic_to_node_map.begin(), apic_to_node_map.end(), NO_NID);
  std::fill(pxm_to_node_map.begin(), pxm_to_node_map.end(), NO_NID);
  std::fill(node_to_pxm_map.begin(), node_to_pxm_map.end(), -1);

  auto status =
      AcpiInitializeTables(initial_table_storage, ACPI_MAX_INIT_TABLES, false);
  if (ACPI_FAILURE(status)) {
    kprintf("AcpiInitializeTables Failed\n");
    kabort();
  }

  ACPI_TABLE_HEADER *madt;
  status = AcpiGetTable((char *)ACPI_SIG_MADT, 1, &madt);
  if (ACPI_FAILURE(status)) {
    kprintf("Failed to locate MADT\n");
    kabort();
  }
  parse_madt(reinterpret_cast<ACPI_TABLE_MADT *>(madt));

  ACPI_TABLE_HEADER *srat;
  status = AcpiGetTable((char *)ACPI_SIG_SRAT, 1, &srat);
  if (ACPI_FAILURE(status)) {
    kprintf("Failed to locate SRAT\n");
    kabort();
  }
  parse_srat(reinterpret_cast<ACPI_TABLE_SRAT *>(srat));
}

// void init() {
//   auto status = AcpiInitializeSubsystem();
//   if (ACPI_FAILURE(status)) {
//     kprintf("AcpiInitializeSubsystem failed: %s\n",
//             AcpiFormatException(status));
//     kabort();
//   }
//   status = AcpiInitializeTables(nullptr, 16, false);
//   if (ACPI_FAILURE(status)) {
//     kprintf("ACPI Initialization Failed\n");
//     kabort();
//   }
//   status = AcpiLoadTables();
//   if (ACPI_FAILURE(status)) {
//     kprintf("AcpiLoadTables failed: %s\n", AcpiFormatException(status));
//     kabort();
//   }
//   status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
//   if (ACPI_FAILURE(status)) {
//     kprintf("AcpiEnableSubsystem failed: %s\n", AcpiFormatException(status));
//     kabort();
//   }
//   status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
//   if (ACPI_FAILURE(status)) {
//     kprintf("AcpiInitializeObjects failed: %s\n",
// AcpiFormatException(status));
//     kabort();
//   }

//   // Use this to enumerate Acpi devices
//   // AcpiGetDevices(nullptr, add_device, nullptr, nullptr);
// }

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
  //*OutHandle = new std::mutex();
  UNIMPLEMENTED();
  return AE_OK;
}

void AcpiOsDeleteLock(ACPI_SPINLOCK Handle) { UNIMPLEMENTED(); }

ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK Handle) {
  // auto mut = static_cast<std::mutex *>(Handle);
  // mut->lock();
  UNIMPLEMENTED();
  return 0;
}

void AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS Flags) {
  // auto mut = static_cast<std::mutex *>(Handle);
  // mut->unlock();
  UNIMPLEMENTED();
}

ACPI_STATUS
AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits,
                      ACPI_SEMAPHORE *OutHandle) {
  // *OutHandle = new semaphore(InitialUnits);
  UNIMPLEMENTED();
  return AE_OK;
}

ACPI_STATUS
AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle) {
  UNIMPLEMENTED();
  return AE_OK;
}

ACPI_STATUS
AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units, UINT16 Timeout) {
  // if (Handle == nullptr) {
  //   return AE_BAD_PARAMETER;
  // }
  // auto sem = static_cast<semaphore *>(Handle);
  // switch (Timeout) {
  // case ACPI_DO_NOT_WAIT:
  //   return sem->trywait(Units) ? AE_OK : AE_TIME;
  // case ACPI_WAIT_FOREVER:
  //   sem->wait(Units);
  //   return AE_OK;
  // default:
  //   UNIMPLEMENTED();
  // }
  UNIMPLEMENTED();
}

ACPI_STATUS
AcpiOsSignalSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units) {
  // if (Handle == nullptr) {
  //   return AE_BAD_PARAMETER;
  // }
  // auto sem = static_cast<semaphore *>(Handle);
  // sem->signal(Units);
  // return AE_OK;
  UNIMPLEMENTED();
  return AE_OK;
}

void *AcpiOsAllocate(ACPI_SIZE Size) {
  // return malloc(Size);
  UNIMPLEMENTED();
  return nullptr;
}

void AcpiOsFree(void *Memory) {
  // free(Memory);
  UNIMPLEMENTED();
}

void *AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS Where, ACPI_SIZE Length) {
  if (!initialized) {
    early_map_memory(Where, Length);
    return reinterpret_cast<void *>(Where);
  } else {
    UNIMPLEMENTED();
    return nullptr;
  }
  // uint64_t paddr = align_down(Where, pmem::page_size);
  // auto diff = Where - paddr;
  // size_t len = align_up(Length + diff, pmem::page_size);
  // auto ret = vmem::allocate_page(len);
  // if (ret == 0) {
  //   kprintf("Failed to allocate virtual pages for ACPI\n");
  //   kabort();
  // }
  // vmem::map(ret, paddr, len);
  // return reinterpret_cast<void *>(ret + diff);
}

void AcpiOsUnmapMemory(void *LogicalAddress, ACPI_SIZE Size) {
  // auto logaddr = reinterpret_cast<uint64_t>(LogicalAddress);
  // uint64_t vaddr = align_down(logaddr, pmem::page_size);
  // auto diff = logaddr - vaddr;
  // uint64_t len = align_up(Size + diff, pmem::page_size);
  // vmem::free_page_range(vaddr, len);
  if (!initialized) {
    early_unmap_memory(reinterpret_cast<uint64_t>(LogicalAddress), Size);
  } else {
    UNIMPLEMENTED();
  }
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
  // kprintf("%s: Unimplemented, but continuing\n", __PRETTY_FUNCTION__);
  UNIMPLEMENTED();
  return AE_OK;
}

ACPI_STATUS
AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber,
                             ACPI_OSD_HANDLER ServiceRoutine) {
  UNIMPLEMENTED();
  return AE_OK;
}

ACPI_THREAD_ID
AcpiOsGetThreadId(void) {
  // return 1;
  UNIMPLEMENTED();
  return AE_OK;
}

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
  // switch (Width) {
  // case 8:
  //   *Value = in8(Address);
  //   break;
  // case 16:
  //   *Value = in16(Address);
  //   break;
  // case 32:
  //   *Value = in32(Address);
  //   break;
  // default:
  //   return AE_BAD_PARAMETER;
  // }
  // return AE_OK;
  UNIMPLEMENTED();
  return AE_OK;
}

ACPI_STATUS
AcpiOsWritePort(ACPI_IO_ADDRESS Address, UINT32 Value, UINT32 Width) {
  // switch (Width) {
  // case 8:
  //   out8(Address, Value);
  //   break;
  // case 16:
  //   out16(Address, Value);
  //   break;
  // case 32:
  //   out32(Address, Value);
  //   break;
  // default:
  //   return AE_BAD_PARAMETER;
  // }
  // return AE_OK;
  UNIMPLEMENTED();
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
  // switch (Width) {
  // case 8:
  //   *(uint8_t *)Value =
  //       pci::read_config8(PciId->Bus, PciId->Device, PciId->Function, Reg);
  //   break;
  // case 16:
  //   *(uint16_t *)Value =
  //       pci::read_config16(PciId->Bus, PciId->Device, PciId->Function, Reg);
  //   break;
  // case 32:
  //   *(uint32_t *)Value =
  //       pci::read_config32(PciId->Bus, PciId->Device, PciId->Function, Reg);
  //   break;
  // }
  // return AE_OK;
  UNIMPLEMENTED();
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
  // uint32_t newval;
  // uint32_t oldval;

  // do {
  //   oldval = *lock;
  //   newval = ((oldval & ~GL_BIT_MASK) | GL_BIT_OWNED) |
  //            ((oldval >> 1) & GL_BIT_PENDING);
  //   __sync_lock_test_and_set(lock, newval);
  // } while (*lock == oldval);
  // return ((newval < GL_BIT_MASK) ? GL_ACQUIRED : GL_BUSY);
  UNIMPLEMENTED();
  return 0;
}

int AcpiOsReleaseGlobalLock(uint32_t *lock) {
  // uint32_t newval;
  // uint32_t oldval;
  // do {
  //   oldval = *lock;
  //   newval = oldval & ~GL_BIT_MASK;
  //   __sync_lock_test_and_set(lock, newval);
  // } while (*lock == oldval);
  // return oldval & GL_BIT_PENDING;
  UNIMPLEMENTED();
  return 0;
}

// namespace ebbrt {
// ACPI_STATUS add_device(ACPI_HANDLE handle, uint32_t level, void *context,
//                        void **ret_val) {
//   ACPI_BUFFER name;

//   name.Pointer = nullptr;
//   name.Length = ACPI_ALLOCATE_BUFFER;

//   auto status = AcpiGetName(handle, ACPI_FULL_PATHNAME, &name);
//   if (ACPI_SUCCESS(status)) {
//     kprintf("%s\n", name.Pointer);
//   }

//   ACPI_DEVICE_INFO *info;
//   status = AcpiGetObjectInfo(handle, &info);
//   if (ACPI_SUCCESS(status)) {
//     if (info->Valid & ACPI_VALID_HID) {
//       kprintf("\tHID: %s\n", info->HardwareId.String);
//     }
//     if (info->Valid & ACPI_VALID_CID) {
//       for (size_t i = 0; i < info->CompatibleIdList.Count; ++i) {
//         kprintf("\tCID: %s\n", info->CompatibleIdList.Ids[i].String);
//       }
//     }
//     if (info->Valid & ACPI_VALID_ADR) {
//       kprintf("\tADR: %.8X\n", info->Address);
//     }
//   }

//   return AE_OK;
// }
