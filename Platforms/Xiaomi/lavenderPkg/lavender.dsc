##
#  Copyright (c) 2011 - 2022, ARM Limited. All rights reserved.
#  Copyright (c) 2014, Linaro Limited. All rights reserved.
#  Copyright (c) 2015 - 2020, Intel Corporation. All rights reserved.
#  Copyright (c) 2018, Bingxing Wang. All rights reserved.
#  Copyright (c) Microsoft Corporation.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
##

################################################################################
#
# Defines Section - statements that will be processed to create a Makefile.
#
################################################################################
[Defines]
  PLATFORM_NAME                  = lavender
  PLATFORM_GUID                  = DAA08544-071F-4A77-BD10-2D2EC946EA2A
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/lavenderPkg
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = RELEASE|DEBUG
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = lavenderPkg/lavender.fdf
  USE_CUSTOM_DISPLAY_DRIVER      = 1

!include StarlordPkg/StarlordPkg.dsc.inc

!ifdef $(UNIXNU_BUILD)
[BuildOptions]
  # Keep the copied FDT and up-to-20 MiB FAT payload disjoint within the
  # already reserved Kernel window; see BootHandoffCopiesWithin().
  *_CLANGPDB_AARCH64_CC_FLAGS = -DUNIXNU_BUILD -DLAVENDER_STAGE1 -DUNIXNU_HANDOFF_BASE=0x80000000ULL -DUNIXNU_HANDOFF_END=0x85800000ULL -DUNIXNU_FDT_COPY_BASE=0x85400000ULL -DUNIXNU_INITRD_COPY_BASE=0x84000000ULL
!endif

[PcdsFixedAtBuild]
  #
  # DDR Memory
  #
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0x80000000

  #
  # UEFI Stack
  #
  gArmPlatformTokenSpaceGuid.PcdCPUCoresStackBase|0x9FF90000
  gArmPlatformTokenSpaceGuid.PcdCPUCorePrimaryStackSize|0x40000

  #
  # SMBIOS
  #
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemManufacturer|"Xiaomi"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemModel|"Redmi Note 7"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemRetailModel|"lavender"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemRetailSku|"Redmi_Note_7_lavender"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemBoardModel|"Redmi Note 7"

  #
  # Simple Frame Buffer
  #
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferWidth|1080
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferHeight|2340
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferColorDepth|32

  #
  # Platform PEI
  #
  gQcomPkgTokenSpaceGuid.PcdPlatformType|"LA"

!ifdef $(UNIXNU_BUILD)
  gUniXNUPkgTokenSpaceGuid.PcdUniXNUDeviceModel|"lavender"
!endif

[LibraryClasses]
  #
  # Memory Libraries
  #
  MemoryMapLib|lavenderPkg/Library/MemoryMapLib/MemoryMapLib.inf

  #
  # QCOM Libraries
  #
  ConfigurationMapLib|lavenderPkg/Library/ConfigurationMapLib/ConfigurationMapLib.inf

!ifdef $(UNIXNU_BUILD)
  UniXNULib|UniXNUPkg/Library/UniXNULib/UniXNULib.inf
!endif

[Components]
  #
  # ACPI Tables
  #
  lavender/AcpiTables.inf

!ifdef $(UNIXNU_BUILD)
  UniXNUPkg/Application/UniXNU/UniXNU.inf
  UniXNUPkg/Driver/UniXNUDxe/UniXNUDxe.inf
  UniXNUPkg/Driver/UniXNURamDiskDxe/UniXNURamDiskDxe.inf
  DarwinBootPkg/Driver/DarwinBoot.inf

  # Built for the FAT payload, rather than embedded in the firmware volume.
  LavenderBringupPkg/Application/LavenderProbe/LavenderProbe.inf
  LavenderBringupPkg/Driver/LavenderHandoffDxe/LavenderHandoffDxe.inf
!endif
