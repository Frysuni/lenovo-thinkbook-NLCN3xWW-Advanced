# ThinkBook Advanced BIOS Menu Unlock

Restore the hidden **Advanced** tab (and other hidden FormSets) in the Lenovo
BIOS Setup on Insyde/Byosoft firmware that removed it — **at runtime, without
reflashing or downgrading**, using the laptop's own unmodified
`SetupUtilityApp`.

Developed and verified on the **Lenovo ThinkBook 16 G6+ AHP** (type 21LG, BIOS
family `NLCN`). The same mechanism applies to the ThinkBook 14 G6+ AHP and, via
the included analysis tool, to other models on the same form-browser code.

> **Status:** working and confirmed on hardware (NLCN38WW). The patch is
> volatile — it lives in RAM for one boot and disappears on reset. Flash is
> never written.

---

## TL;DR

```
# In the UEFI Shell, from a USB stick that holds both files:
FS0:\> connect -r                       # make sure the form browser is loaded
FS0:\> BiosAdvancedPatch.efi            # disables the "hide Advanced" filter
FS0:\> SetupUtilityApp.efi              # your laptop's own Setup app — Advanced now shows
```

`BiosAdvancedPatch.efi` finds the resident form-browser driver in memory and
NOPs the single instruction that hides flag=0 FormSets, then reads the bytes
back to prove the change landed. You then launch the **stock** Lenovo
`SetupUtilityApp` and the Advanced tab is there.

See [docs/04-build-and-run.md](docs/04-build-and-run.md) for the full procedure
and the on-screen output to expect.

---

## Why the Advanced tab vanished

Between BIOS `NLCN36WW` and `NLCN37WW`, Lenovo's `H2OFormBrowserDxe` gained one
instruction in its FormSet-sorting routine:

```asm
cmp byte ptr [rbx+0x10], 0   ; read the FormSet's "show" flag
jz  <backward>               ; flag == 0  ->  skip this FormSet
```

The Advanced FormSet has always shipped with `flag = 0`. Older firmware had no
such check and showed it; newer firmware silently drops it from every
`SendForm()` call. The fix is to turn that conditional jump into two `NOP`s, so
every FormSet passes — exactly how the pre-NLCN37 firmware behaved.

Full analysis: [docs/02-root-cause.md](docs/02-root-cause.md).

---

## What's in here

| Path | What it is |
|---|---|
| `patcher/BiosAdvancedPatch.c` | The runtime UEFI patcher (gnu-efi). |
| `patcher/Makefile` | Reproducible build (via the Nix dev shell). |
| `patcher/prebuilt/BiosAdvancedPatch.efi` | Ready-to-run binary; SHA256 in [docs/04](docs/04-build-and-run.md). |
| `patcher/analyze_formbrowser.py` | Offline tool that locates the filter in any firmware build and emits the exact patch. |
| `docs/` | Overview, root cause, reproduction, build/run, version matrix, safety, downloads. |
| `flake.nix` | Nix dev shell with the full toolchain (gnu-efi, capstone, UEFITool, …). |

The patcher is **self-targeting**: it locates `H2OFormBrowserDxe` through the
`EFI_FORM_BROWSER2_PROTOCOL` it owns and scans only that module for the filter
*idiom* (not a fixed byte string), so it adapts to other versions and similar
models. A known-good exact anchor for the G6+ AHP family is kept as a fallback.

---

## Documentation

1. [Overview](docs/01-overview.md) — the problem and the approach.
2. [Root cause](docs/02-root-cause.md) — the instruction, the flag table, the call path.
3. [Reproduce the analysis](docs/03-reproduce-analysis.md) — derive/verify the patch for any firmware.
4. [Build & run](docs/04-build-and-run.md) — build, USB prep, UEFI Shell steps, troubleshooting.
5. [Version matrix](docs/05-cross-version-matrix.md) — which builds hide Advanced, with hashes.
6. [Safety & limitations](docs/06-safety-and-limitations.md) — what it does and does not do.
7. [Firmware downloads](docs/07-firmware-downloads.md) — where to get the BIOS packages.

---

## Safety

This tool only flips one conditional jump in a **copy of the form browser that
already runs in RAM**. It does not write flash, does not change NVRAM, and is
undone by any reset. It cannot brick the machine. Read
[docs/06-safety-and-limitations.md](docs/06-safety-and-limitations.md) before
use, and understand that changing hidden Advanced/CBS settings can still make
your system unbootable — that risk is on you, not on the patch.

## License

[MIT](LICENSE). Contains no Lenovo or Insyde code; firmware must be obtained
from the vendor.
