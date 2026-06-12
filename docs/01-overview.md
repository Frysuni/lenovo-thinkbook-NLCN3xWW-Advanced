# 1. Overview

## The problem

Lenovo ThinkBook laptops on the Insyde/Byosoft `NLCN` BIOS family ship a Setup
application (`SetupUtilityApp`) that, in the factory configuration, shows only a
handful of tabs — typically **Main**, **Security**, **Boot**, and **Power**. A
much larger **Advanced** menu (CPU/AMD CBS, memory, chipset, debug options)
exists in the firmware but is not shown.

On *older* BIOS builds the Advanced tab could be reached (it was simply present
in the Setup UI). Starting with **NLCN37WW**, it disappeared. Downgrading or
reflashing to expose it is risky and often blocked, and third-party "HII
launcher" apps that try to open the Advanced FormSet directly tend to produce
black screens or corrupted output on this firmware.

## The approach

This project does **not** reflash, downgrade, modify NVRAM, or replace the Setup
app. It makes one surgical change to the form-browser driver **while it is
already running in RAM**, then launches the laptop's own unmodified
`SetupUtilityApp`. The change:

- is a single instruction turned into `NOP`s,
- lives only for the current boot (gone on reset),
- never touches the SPI flash,
- keeps the normal Setup code path intact (same `SendForm()` call, same
  handle list, same everything — only the hidden-tab filter is disabled).

## How it works in one paragraph

The form browser (`H2OFormBrowserDxe`) keeps an internal table that marks each
BIOS FormSet as "show" (flag = 1) or "hide" (flag = 0). Its sorting routine
contains a check that *skips* every flag = 0 FormSet. The Advanced FormSet is
flagged 0, so it is dropped from the UI. The runtime patcher locates that
driver in memory (via the `EFI_FORM_BROWSER2_PROTOCOL` it publishes), finds the
skip instruction, and replaces the conditional jump with two `NOP`s. With the
skip gone, every FormSet — Advanced included — is shown, exactly as on the
pre-NLCN37 firmware.

## What you need

- The target laptop (or a same-family model — see
  [the version matrix](05-cross-version-matrix.md)).
- A FAT32 USB stick.
- The prebuilt `BiosAdvancedPatch.efi` (or build it yourself).
- Your laptop's own `SetupUtilityApp` EFI (extracted from its firmware — see
  [build & run](04-build-and-run.md)).
- Access to the UEFI Shell (the firmware's built-in shell, or `shellx64.efi` on
  the stick).

Continue to [the root-cause analysis](02-root-cause.md) for the full technical
story, or jump to [build & run](04-build-and-run.md) to use it.
