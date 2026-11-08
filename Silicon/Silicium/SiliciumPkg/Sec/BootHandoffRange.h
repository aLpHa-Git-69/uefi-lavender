/** @file
  Overflow-safe containment check for bootloader-owned memory.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef BOOT_HANDOFF_RANGE_H
#define BOOT_HANDOFF_RANGE_H

STATIC inline
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

// Two independently reserved windows, with no reliance on the intervening RAM.
// Each range is contained before its end address is used in the overlap test.
STATIC inline
BOOLEAN
BootHandoffCopiesAcrossWindows (
  IN UINT64  FdtBase,
  IN UINT64  FdtSize,
  IN UINT64  FdtWindowBase,
  IN UINT64  FdtWindowEnd,
  IN UINT64  InitrdBase,
  IN UINT64  InitrdSize,
  IN UINT64  InitrdWindowBase,
  IN UINT64  InitrdWindowEnd
  )
{
  return ((InitrdBase % 512) == 0) && ((InitrdSize % 512) == 0) &&
         BootHandoffRangeWithin (FdtBase, FdtSize, FdtWindowBase, FdtWindowEnd) &&
         BootHandoffRangeWithin (InitrdBase, InitrdSize, InitrdWindowBase, InitrdWindowEnd) &&
         ((InitrdBase + InitrdSize <= FdtBase) || (FdtBase + FdtSize <= InitrdBase));
}

// The original small-payload profile keeps both copies in the Kernel window.
// lavender's 0x84000000..0x85400000 slot is exactly 20 MiB.
STATIC inline
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
  return (InitrdBase < FdtBase) &&
         BootHandoffCopiesAcrossWindows (FdtBase, FdtSize, WindowBase, WindowEnd,
           InitrdBase, InitrdSize, WindowBase, WindowEnd);
}

#endif
