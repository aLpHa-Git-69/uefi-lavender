//
// Copyright (c) 2011 - 2025, Arm Limited. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause-Patent
//

#include <AsmMacroLib.h>

#include <Library/ArmLib.h>

//
// SCTRL_EL1 Bits
//
.set CTRL_SPAN_BIT, BIT23

ASM_FUNC (_ModuleEntryPoint)
  // Preserve the Linux bootloader's x0 (FDT address) across initialization.
  // x20 is callee-saved; the platform initialization routines use the C ABI.
  mov   x20, x0
  mov   x21, x0
  mov   x22, xzr
  mov   x23, xzr

  // Some ABL revisions pass a virtual FDT address while their MMU is still
  // enabled. Resolve it using the inherited translation tables before SEC
  // installs the platform's identity mapping. Keep the original arg, SCTLR
  // and PAR for the visible handoff diagnostic.
  mrs   x9, CurrentEL
  cmp   x9, #4
  b.eq  .LHandoffEl1
  cmp   x9, #8
  b.ne  .LHandoffDone
  mrs   x22, sctlr_el2
  tbz   x22, #0, .LHandoffDone
  at    s1e2r, x20
  b     .LHandoffTranslated
.LHandoffEl1:
  mrs   x22, sctlr_el1
  tbz   x22, #0, .LHandoffDone
  at    s1e1r, x20
.LHandoffTranslated:
  isb
  mrs   x23, par_el1
  tbnz  x23, #0, .LHandoffDone
  and   x9, x23, #0x000ffffffffff000
  and   x10, x20, #0xfff
  orr   x20, x9, x10
.LHandoffDone:
  mov   x0, x21
  msr   SPSel, #1
  isb

  // Do Platform Specific Initialization
  bl    ASM_PFX (PlatformAssemblyInitialize)

  // Disable SPAN
  EL1_OR_EL2 (x0)
1:mrs   x0, sctlr_el1
  orr   x0, x0, #CTRL_SPAN_BIT
  msr   sctlr_el1, x0
2:nop

  // Enable Floating Point
  bl    ASM_PFX (ArmEnableVFP)

  // Setup Exception Vector
  EL1_OR_EL2 (x0)
1:bl    ASM_PFX (SetupExceptionLevel1)
  b     ASM_PFX (MainEntryPoint)
2:bl    ASM_PFX (SetupExceptionLevel2)
  b     ASM_PFX (MainEntryPoint)

ASM_PFX (MainEntryPoint):
  // Get Stack Details
  MOV64 (x0, FixedPcdGet64 (PcdCPUCoresStackBase))
  MOV32 (x1, FixedPcdGet32 (PcdCPUCorePrimaryStackSize))

  // Set Stack End Address
  add   x2, x0, x1

  // Set Stack Pointer
  mov   sp, x2

  // Clear Stack
  mov   x3, xzr
0:stp   x3, x3, [x0], #16
  cmp   x0, x2
  b.lt  0b

  // Set Jump Location
  ldr   x4, =ASM_PFX (SecEntry)

  // Pass the original bootloader argument to C.
  mov   x0, x20
  mov   x1, x21
  mov   x2, x22
  mov   x3, x23

  // Clear Frame Pointer
  mov   x29, xzr

  // Jump to C Code
  blr   x4

  // Do Infinite Spin
  b     .
