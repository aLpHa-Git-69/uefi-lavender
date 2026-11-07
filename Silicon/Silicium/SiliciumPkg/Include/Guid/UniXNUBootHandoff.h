/** @file
  Diagnostic record for the bootloader FDT/initrd handoff.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#ifndef UNIXNU_BOOT_HANDOFF_H
#define UNIXNU_BOOT_HANDOFF_H

#define UNIXNU_BOOT_HANDOFF_DIAGNOSTIC_GUID \
  { 0x97C43BF7, 0xD25A, 0x4188, { 0xB0, 0x41, 0x61, 0x66, 0xB4, 0xB6, 0xE8, 0x11 } }

typedef struct {
  UINT64  EntryArg;
  UINT32  Status;
  UINT32  FdtBytes;
  UINT64  InitrdStart;
  UINT64  InitrdEnd;
  UINT64  RawEntryArg;
  UINT64  EntrySctlr;
  UINT64  EntryPar;
} UNIXNU_BOOT_HANDOFF_DIAGNOSTIC;

// Status: 0 published, 1 unmapped argument, 2 bad FDT header, 3 FDT range,
// 4 missing chosen, 5 missing initrd, 6 malformed properties, 7 initrd range.
#endif
