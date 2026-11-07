/**
  Copyright (c) 2011-2017, ARM Limited. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/PcdLib.h>
#include <Library/DebugLib.h>
#include <Library/PrePiLib.h>
#include <Library/DebugAgentLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/SerialPortLib.h>
#include <Library/PlatformSecLib.h>
#include <Library/PrePiHobListPointerLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/TimerLib.h>
#include <Library/HobLib.h>

#include "Sec.h"

#ifdef UNIXNU_BUILD
#include <Library/BaseLib.h>
#include <Library/FdtLib.h>
#include <Guid/UniXNUBootHandoff.h>
#include "BootHandoffRange.h"

// Contract consumed by UniXNU's shared UniXNURamDiskDxe.
STATIC CONST EFI_GUID mUniXNUEntryArgHobGuid = {
  0xDAEE82E6, 0x8090, 0x4E72, { 0xBF, 0xB9, 0x78, 0xC1, 0x3C, 0xD4, 0xE3, 0x59 }
};
STATIC CONST EFI_GUID mHandoffDiagnosticGuid = UNIXNU_BOOT_HANDOFF_DIAGNOSTIC_GUID;

STATIC
UINT64
PrepareBootHandoff (
  IN UINT64  IncomingArg
  )
{
  CONST VOID  *Incoming;
  VOID        *Copy;
  CONST VOID  *StartProp;
  CONST VOID  *EndProp;
  UINT64      Start;
  UINT64      End;
  UINT64      Length;
  UINT64      Cell64;
  UINT32      Cell32;
  UINT32      FdtBytes;
  INT32       Chosen;
  INT32       StartLength;
  INT32       EndLength;

  // ABL can hand over its FDT in a low bootloader RAM area (this phone
  // reports 0x44ffe000 with the MMU off). Read it before MemoryPeim replaces
  // that addressability with the platform map, and retain a bounded copy in
  // the already reserved Kernel window. Do not infer an address offset.
  if (((IncomingArg & 3) != 0) || (IncomingArg < 0x40000000ULL) ||
      (IncomingArg > 0xA0000000ULL - 40))
  {
    return IncomingArg;
  }

  Incoming = (CONST VOID *)(UINTN)IncomingArg;
  if (FdtCheckHeader (Incoming) != 0) {
    return IncomingArg;
  }

  FdtBytes = FdtTotalSize (Incoming);
  if ((FdtBytes < 40) || (FdtBytes > SIZE_1MB) ||
      (FdtBytes > 0xA0000000ULL - IncomingArg) ||
      !BootHandoffRangeWithin (UNIXNU_FDT_COPY_BASE, FdtBytes, UNIXNU_HANDOFF_BASE, UNIXNU_HANDOFF_END))
  {
    return IncomingArg;
  }

  Copy = (VOID *)(UINTN)UNIXNU_FDT_COPY_BASE;
  CopyMem (Copy, Incoming, FdtBytes);
  Chosen = FdtPathOffset (Copy, "/chosen");
  if (Chosen < 0) {
    return UNIXNU_FDT_COPY_BASE;
  }

  StartProp = FdtGetProp (Copy, Chosen, "linux,initrd-start", &StartLength);
  EndProp   = FdtGetProp (Copy, Chosen, "linux,initrd-end", &EndLength);
  if ((StartProp == NULL) || (EndProp == NULL) ||
      ((StartLength != 4) && (StartLength != 8)) ||
      ((EndLength != 4) && (EndLength != 8)))
  {
    return UNIXNU_FDT_COPY_BASE;
  }

  Start = (StartLength == 8) ? Fdt64ToCpu (ReadUnaligned64 (StartProp)) : Fdt32ToCpu (ReadUnaligned32 (StartProp));
  End   = (EndLength == 8) ? Fdt64ToCpu (ReadUnaligned64 (EndProp)) : Fdt32ToCpu (ReadUnaligned32 (EndProp));
  if ((End <= Start) || (Start < 0x40000000ULL) || (End > 0xA0000000ULL)) {
    return UNIXNU_FDT_COPY_BASE;
  }

  Length = End - Start;
  // The OS FAT payload can fill the 20 MiB reserved slot below the FDT.
  // Keep both copies disjoint and contained before touching payload memory.
  if (!BootHandoffCopiesWithin (UNIXNU_FDT_COPY_BASE, FdtBytes,
        UNIXNU_INITRD_COPY_BASE, Length, UNIXNU_HANDOFF_BASE, UNIXNU_HANDOFF_END))
  {
    return UNIXNU_FDT_COPY_BASE;
  }

  CopyMem ((VOID *)(UINTN)UNIXNU_INITRD_COPY_BASE, (CONST VOID *)(UINTN)Start, (UINTN)Length);
  if (StartLength == 8) {
    Cell64 = CpuToFdt64 (UNIXNU_INITRD_COPY_BASE);
    if (FdtSetProp (Copy, Chosen, "linux,initrd-start", &Cell64, sizeof (Cell64)) != 0) {
      return UNIXNU_FDT_COPY_BASE;
    }
  } else {
    Cell32 = CpuToFdt32 ((UINT32)UNIXNU_INITRD_COPY_BASE);
    if (FdtSetProp (Copy, Chosen, "linux,initrd-start", &Cell32, sizeof (Cell32)) != 0) {
      return UNIXNU_FDT_COPY_BASE;
    }
  }

  if (EndLength == 8) {
    Cell64 = CpuToFdt64 (UNIXNU_INITRD_COPY_BASE + Length);
    if (FdtSetProp (Copy, Chosen, "linux,initrd-end", &Cell64, sizeof (Cell64)) != 0) {
      return UNIXNU_FDT_COPY_BASE;
    }
  } else {
    Cell32 = CpuToFdt32 ((UINT32)(UNIXNU_INITRD_COPY_BASE + Length));
    if (FdtSetProp (Copy, Chosen, "linux,initrd-end", &Cell32, sizeof (Cell32)) != 0) {
      return UNIXNU_FDT_COPY_BASE;
    }
  }

  return UNIXNU_FDT_COPY_BASE;
}

STATIC
VOID
PublishBootHandoff (
  IN UINT64  EntryArg,
  IN UINT64  RawEntryArg,
  IN UINT64  EntrySctlr,
  IN UINT64  EntryPar
  )
{
  CONST VOID  *Fdt;
  CONST VOID  *StartProp;
  CONST VOID  *EndProp;
  UINT64      FdtSize;
  UINT64      Start;
  UINT64      End;
  INT32       Chosen;
  INT32       StartLength;
  INT32       EndLength;
  UNIXNU_BOOT_HANDOFF_DIAGNOSTIC  Record;
  UNIXNU_BOOT_HANDOFF_DIAGNOSTIC  *Diagnostic;

  ZeroMem (&Record, sizeof (Record));
  Record.EntryArg = EntryArg;
  Record.Status   = 1;
  Record.RawEntryArg = RawEntryArg;
  Record.EntrySctlr  = EntrySctlr;
  Record.EntryPar    = EntryPar;
  Diagnostic = BuildGuidDataHob (&mHandoffDiagnosticGuid, &Record, sizeof (Record));

  // Check the address before dereferencing it. This window is mapped and
  // EfiReservedMemoryType in lavender's MemoryMapLib, before any DXE allocation.
  if (((EntryArg & 3) != 0) ||
      !BootHandoffRangeWithin (EntryArg, 40, UNIXNU_HANDOFF_BASE, UNIXNU_HANDOFF_END))
  {
    DEBUG ((DEBUG_ERROR, "LAVENDERHOB: x0 0x%lx outside the reserved handoff window\n", EntryArg));
    return;
  }

  Fdt = (CONST VOID *)(UINTN)EntryArg;
  Diagnostic->Status = 2;
  if (FdtCheckHeader (Fdt) != 0) {
    DEBUG ((DEBUG_ERROR, "LAVENDERHOB: x0 is not an FDT\n"));
    return;
  }

  FdtSize = FdtTotalSize (Fdt);
  Diagnostic->FdtBytes = (UINT32)FdtSize;
  Diagnostic->Status   = 3;
  if ((FdtSize < 40) ||
      !BootHandoffRangeWithin (EntryArg, FdtSize, UNIXNU_HANDOFF_BASE, UNIXNU_HANDOFF_END))
  {
    DEBUG ((DEBUG_ERROR, "LAVENDERHOB: FDT extends outside reserved memory\n"));
    return;
  }

  Chosen = FdtPathOffset (Fdt, "/chosen");
  Diagnostic->Status = (Chosen < 0) ? 4 : 5;
  if (Chosen >= 0) {
    StartProp = FdtGetProp (Fdt, Chosen, "linux,initrd-start", &StartLength);
    EndProp   = FdtGetProp (Fdt, Chosen, "linux,initrd-end", &EndLength);
    if ((StartProp != NULL) || (EndProp != NULL)) {
      Diagnostic->Status = 6;
      if ((StartProp == NULL) || (EndProp == NULL) ||
          ((StartLength != 4) && (StartLength != 8)) ||
          ((EndLength != 4) && (EndLength != 8)))
      {
        DEBUG ((DEBUG_ERROR, "LAVENDERHOB: malformed initrd properties\n"));
        return;
      }

      Start = (StartLength == 8) ? Fdt64ToCpu (ReadUnaligned64 (StartProp)) : Fdt32ToCpu (ReadUnaligned32 (StartProp));
      End   = (EndLength == 8) ? Fdt64ToCpu (ReadUnaligned64 (EndProp)) : Fdt32ToCpu (ReadUnaligned32 (EndProp));
      Diagnostic->InitrdStart = Start;
      Diagnostic->InitrdEnd   = End;
      Diagnostic->Status     = 7;
      if ((End <= Start) || ((End - Start) % 512 != 0) ||
          !BootHandoffRangeWithin (Start, End - Start, UNIXNU_HANDOFF_BASE, UNIXNU_HANDOFF_END) ||
          ((Start < EntryArg + FdtSize) && (EntryArg < End)))
      {
        DEBUG ((DEBUG_ERROR, "LAVENDERHOB: initrd 0x%lx-0x%lx is not a protected, disjoint sector range\n", Start, End));
        return;
      }

      DEBUG ((DEBUG_ERROR, "LAVENDERHOB: reserved initrd 0x%lx-0x%lx\n", Start, End));
      Diagnostic->Status = 0;
    }
  }

  BuildGuidDataHob (&mUniXNUEntryArgHobGuid, &EntryArg, sizeof (EntryArg));
  DEBUG ((DEBUG_ERROR, "LAVENDERHOB: preserved FDT 0x%lx (%lu bytes)\n", EntryArg, FdtSize));
}
#endif

#ifndef MDEPKG_NDEBUG
STATIC
VOID
ClearFrameBuffer (VOID)
{
  EFI_MEMORY_REGION_DESCRIPTOR FrameBufferRegion;

  // Locate "Display Reserved" Memory Region
  LocateMemoryRegionByName ("Display Reserved", &FrameBufferRegion);
  LocateMemoryRegionByName ("Display_Reserved", &FrameBufferRegion);

  // Clear Memory Region
  if (FrameBufferRegion.Address != 0 && FrameBufferRegion.Length != 0) {
    ZeroMem ((VOID *)FrameBufferRegion.Address, FrameBufferRegion.Length);
  }
}

STATIC
VOID
PrintFirmwareVersion (VOID)
{
  // Clear Frame Buffer
  ClearFrameBuffer ();

  // Print UEFI Version Message
  DEBUG ((EFI_D_WARN, "\n"));
  DEBUG ((EFI_D_WARN, "Project Silicium for %a %a\n",            FixedPcdGetPtr (PcdSmbiosSystemManufacturer), FixedPcdGetPtr (PcdSmbiosSystemModel)));
  DEBUG ((EFI_D_WARN, "Firmware Version %s Build at %a on %a\n", PcdGetPtr (PcdFirmwareVersionString), __TIME__, __DATE__));
  DEBUG ((EFI_D_WARN, "\n"));
}
#endif

STATIC
EFI_STATUS
InitializeMemory (VOID)
{
  EFI_STATUS                    Status;
  EFI_MEMORY_REGION_DESCRIPTOR  UefiMemoryRegion;
  EFI_HOB_HANDOFF_INFO_TABLE   *HobList;

  // Locate "DXE Heap" Memory Region
  LocateMemoryRegionByName ("DXE Heap", &UefiMemoryRegion);
  LocateMemoryRegionByName ("DXE_Heap", &UefiMemoryRegion);

  // Save UEFI Memory Details
  UINTN UefiMemoryBase = UefiMemoryRegion.Address;
  UINTN UefiMemorySize = UefiMemoryRegion.Length;

  // Verify UEFI Memory Base
  if (UefiMemoryBase == 0 && UefiMemorySize == 0) {
    DEBUG ((EFI_D_ERROR, "Failed to Locate \"DXE Heap\" Memory Region!\n"));
    return EFI_NOT_FOUND;
  }

  // Initialize HOB List
  HobList = HobConstructor ((VOID *)UefiMemoryBase, UefiMemorySize, (VOID *)UefiMemoryBase, (VOID *)(UefiMemoryBase + UefiMemorySize));
  PrePeiSetHobList (HobList);

  // Initialize MMU & Memory HOBs
  Status = MemoryPeim (UefiMemoryBase, UefiMemorySize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Build Stack HOB
  BuildStackHob (FixedPcdGet64 (PcdCPUCoresStackBase), FixedPcdGet32 (PcdCPUCorePrimaryStackSize));

  return EFI_SUCCESS;
}

STATIC
VOID
SecMain (IN UINT64 EntryArg, IN UINT64 RawEntryArg, IN UINT64 EntrySctlr, IN UINT64 EntryPar)
{
  EFI_STATUS Status;

#ifdef UNIXNU_BUILD
  // The incoming address must be consumed while the bootloader handoff is
  // still accessible, before initializing the platform MMU and DXE heap.
  EntryArg = PrepareBootHandoff (RawEntryArg);
#endif

  // Do Platform Specific Initialization
  PlatformInitialize ();

#ifndef MDEPKG_NDEBUG
  // Initialize Serial Port
  Status = SerialPortInitialize ();
  if (EFI_ERROR (Status)) {
    return;
  }

  // Print Firmware Version
  PrintFirmwareVersion ();

  // Initialize Debug Agent
  InitializeDebugAgent (DEBUG_AGENT_INIT_POSTMEM_SEC, NULL, NULL);
  SaveAndSetDebugTimerInterrupt (TRUE);
#endif

  // Initialize Memory
  Status = InitializeMemory ();
  if (EFI_ERROR (Status)) {
    return;
  }

  // Build CPU HOB
  BuildCpuHob (ArmGetPhysicalAddressBits (), PcdGet8 (PcdPrePiCpuIoSize));

  // Set the Boot Mode
  SetBootMode (BOOT_WITH_DEFAULT_SETTINGS);

  // Initialize Platform HOBs
  Status = PlatformPeim ();
  if (EFI_ERROR (Status)) {
    return;
  }

#ifdef UNIXNU_BUILD
  PublishBootHandoff (EntryArg, RawEntryArg, EntrySctlr, EntryPar);
#endif

  // Run Library Cconstructors
  ProcessLibraryConstructorList ();

  // Decompress all FVs
  Status = DecompressFirstFv ();
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to Decompress FV! Status = %r\n", Status));
    return;
  }

  // Load DXE Core
  Status = LoadDxeCoreFromFv (NULL, 0);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to Load DXE Core! Status = %r\n", Status));
  }
}

VOID
SecEntry (IN UINT64 EntryArg, IN UINT64 RawEntryArg, IN UINT64 EntrySctlr, IN UINT64 EntryPar)
{
  // Disable Data Cache
  ArmDisableDataCache ();

  // Invalidate Instruction Cache
  ArmInvalidateInstructionCache ();

  // Enable Instruction Cache
  ArmEnableInstructionCache ();

  // Invalidate Stack & UEFI FD D-Cache
  WriteBackInvalidateDataCacheRange ((VOID *)FixedPcdGet64 (PcdCPUCoresStackBase), FixedPcdGet32 (PcdCPUCorePrimaryStackSize));
  WriteBackInvalidateDataCacheRange ((VOID *)FixedPcdGet64 (PcdFdBaseAddress),     FixedPcdGet32 (PcdFdSize));

  // Verify Exception Vector Table
  ASSERT (((UINTN)SecVectorTable & ARM_VECTOR_TABLE_ALIGNMENT) == 0);

  // Enable new Exception Vector Table
  ArmWriteVBar ((UINTN)SecVectorTable);

  // Enter Main SEC Function
  SecMain (EntryArg, RawEntryArg, EntrySctlr, EntryPar);
}
