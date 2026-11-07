/** @file
  Overflow-safe containment check for bootloader-owned memory.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef BOOT_HANDOFF_RANGE_H
#define BOOT_HANDOFF_RANGE_H

STATIC
BOOLEAN
BootHandoffRangeWithin (
  IN UINT64  Base,
  IN UINT64  Size,
  IN UINT64  WindowBase,
  IN UINT64  WindowEnd
  )
{
  return (Size != 0) && (WindowBase < WindowEnd) &&
         (Base >= WindowBase) && (Base < WindowEnd) &&
         (Size <= WindowEnd - Base);
}

// Both copies stay in the platform's existing reserved Kernel window.
// Bound payload size by the copied FDT's base rather than an unrelated
// constant: lavender's 0x84000000..0x85400000 slot is exactly 20 MiB.
STATIC
BOOLEAN
BootHandoffCopiesWithin (
  IN UINT64  FdtBase,
  IN UINT64  FdtSize,
  IN UINT64  InitrdBase,
  IN UINT64  InitrdSize,
  IN UINT64  WindowBase,
  IN UINT64  WindowEnd
  )
{
  return (InitrdBase < FdtBase) && ((InitrdBase % 512) == 0) &&
         ((InitrdSize % 512) == 0) &&
         BootHandoffRangeWithin (FdtBase, FdtSize, WindowBase, WindowEnd) &&
         BootHandoffRangeWithin (InitrdBase, InitrdSize, WindowBase, WindowEnd) &&
         (InitrdSize <= FdtBase - InitrdBase);
}

#endif
