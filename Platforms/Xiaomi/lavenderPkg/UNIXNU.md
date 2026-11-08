# Lavender UniXNU firmware integration

This branch supplies the Redmi Note 7 / SDM660 firmware used by the Lavender
bring-up workspace: <https://github.com/aLpHa-Git-69/UniXNU-lavender>.

The component sources are published separately:

- Firmware: <https://github.com/aLpHa-Git-69/uefi-lavender>
- Boot manager/RAM filesystem: <https://github.com/aLpHa-Git-69/UniXNU>
- XNU loader: <https://github.com/aLpHa-Git-69/DarwinBoot>
- Darwin 22 kernel patches: <https://github.com/aLpHa-Git-69/ios-lavender>

Build through the consuming board workspace, which supplies the pinned
checkouts, `firmware.toml`, and `package/LavenderBringupPkg`.

The active board milestone is Windows ARM64/WinPE; iOS is paused at native
Gold CPU initialization with interrupt-controller/timer discovery. Select
`windows-firmware.toml` in the board builder for the Windows RAM-boot profile.

## Handoff and reservations

ABL's incoming FDT is copied before MemoryPeim changes addressability. SEC
publishes the `DAEE82E6-8090-4E72-BFB9-78C13CD4E359` entry-argument HOB used
by UniXNURamDiskDxe, plus a separate diagnostic HOB.

The existing platform `Kernel` reservation is `0x80000000..0x85800000`:

| Copy | Address/range | Bound |
| --- | --- | --- |
| FDT | `0x85400000` | Valid FDT, 40 bytes to 1 MiB |
| FAT initrd | `0x84000000..0x85400000` | Nonzero complete 512-byte sectors, at most 20 MiB |

`BootHandoffCopiesWithin()` verifies containment and disjointness before
copying the initrd. No new physical RAM region is declared by this policy.
The native kernel's separate ownership window is allocated by DarwinBoot
from the live conventional-memory map.

### Windows PE RAM payload

With both `UNIXNU_BUILD` and `WINDOWS_BUILD` defined, the FDT copy remains at
`0x85400000` and the payload copy uses `0xc0000000..0xd0000000` (256 MiB).
This lies inside the device-measured `0xa0004000..0xfeac0000` conventional
run. The platform adds a reserved system-memory allocation HOB before DXE;
RamManager maps only gaps in the existing map and therefore retains it.

`BootHandoffCopiesAcrossWindows()` independently validates the FDT and
payload windows and disjointness before copying. SEC also validates the
published initrd against that reserved high-RAM window. The board's WinPE
recipe verifies Microsoft's ARM64 WIM/boot files, creates a FAT payload and
checks the complete Android image against the 384 MiB fastboot limit.

Firmware/source and image tests are recorded in the consuming board workspace.
Windows PE hardware execution on this device remains a separate checkpoint.

## Verification status

Hardware Stage 1 reported seven probe checks passed. The subsequent Gold/RAM
probe confirmed PSCI CPU_ON/CPU_OFF, Gold `0x100` at EL1 with 16 KiB support,
and an aligned conventional-memory run `0xa0004000..0xfeac0000` (about
1.48 GiB). The board workspace contains the evidence records.

The 20 MiB copy layout has boundary/overflow host checks. A rebuilt FD and
the larger kernel payload still require build/image and hardware checks.
