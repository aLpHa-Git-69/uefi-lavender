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

## Verification status

Hardware Stage 1 reported seven probe checks passed. The subsequent Gold/RAM
probe confirmed PSCI CPU_ON/CPU_OFF, Gold `0x100` at EL1 with 16 KiB support,
and an aligned conventional-memory run `0xa0004000..0xfeac0000` (about
1.48 GiB). The board workspace contains the evidence records.

The 20 MiB copy layout has boundary/overflow host checks. A rebuilt FD and
the larger kernel payload still require build/image and hardware checks.
