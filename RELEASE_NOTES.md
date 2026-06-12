# BiosAdvancedPatch v5 — ThinkBook Advanced BIOS menu unlock

Restores the hidden **Advanced** tab in the stock Lenovo BIOS Setup at runtime,
without reflashing or downgrading. The patch lives in RAM for one boot and
reverts on reset; flash is never written.

## Supported BIOS versions

| BIOS | Status | `BiosAdvancedPatch.efi` | `SetupUtilityApp.efi` |
|---|---|---|---|
| **NLCN39WW** | covered (byte-identical to 38) | this release | extract from your firmware |
| **NLCN38WW** | confirmed on hardware | this release | extract from your firmware |
| **NLCN37WW** | covered (byte-identical to 38) | this release | extract from your firmware |

NLCN37/38/39 ship a byte-identical form browser, so **one binary covers all
three**. For any other version, run `patcher/analyze_formbrowser.py` on its
`H2OFormBrowserDxe` first (see the docs).

## Downloads

- **`BiosAdvancedPatch.efi`** — attached to this release.
  `SHA256: eb0c1d6e4ded03db4a86bcd26f1cddd5be274ccab31331d1473211d7574346e6`
- **`SetupUtilityApp.efi`** — **not** attached: it is Lenovo's copyrighted
  binary and is not redistributed here. Extract it from your own firmware in one
  command:

  ```bash
  nix develop
  scripts/extract-from-bios.sh BIOS-NLCNxxWW.exe out/
  # -> out/SetupUtilityApp.efi  (plus out/H2OFormBrowserDxe.efi and an analysis)
  ```

## Quick start

```
# Copy BiosAdvancedPatch.efi and your extracted SetupUtilityApp.efi to a
# FAT32 USB stick, boot the UEFI Shell, then:
FS0:\> connect -r
FS0:\> BiosAdvancedPatch.efi      # look for: after: 90 90 -> OK (verified)
FS0:\> SetupUtilityApp.efi        # Advanced tab is now present
```

How to reach the UEFI Shell from Windows/Linux: see the docs.

## Docs

- English: [docs/](docs/README.md) · Русский: [docs/ru/](docs/ru/README.md)
- Build & run: [docs/04](docs/04-build-and-run.md) · UEFI Shell: [docs/08](docs/08-uefi-shell-access.md)
- Root cause: [docs/02](docs/02-root-cause.md) · Safety: [docs/06](docs/06-safety-and-limitations.md)

## Safety

Only one conditional jump in the in-RAM form browser is changed. No flash, no
NVRAM, reverts on reset — it cannot brick the machine. Changing hidden
Advanced/CBS settings afterwards still can; read [docs/06](docs/06-safety-and-limitations.md).
