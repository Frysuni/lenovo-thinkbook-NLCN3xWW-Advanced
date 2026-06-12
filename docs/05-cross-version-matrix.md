# 5. Cross-version matrix

Result of running [`analyze_formbrowser.py`](03-reproduce-analysis.md) against
the decompressed `H2OFormBrowserDxe` PE32 section
(GUID `9E5DAEB4-4B91-4466-9EBE-81C7E4401E6D`) from every BIOS package examined
for the **ThinkBook 14/16 G6+ AHP** (`NLCN` family).

| Version | `H2OFormBrowserDxe` PE32 SHA256 | Advanced hidden? | Filter offset |
|---|---|:--:|:--:|
| NLCN22WW | `ce531517b25d8af42aaaff8efe342a45624eab31b4a8d65d75d89b7534daaeca` | no | — |
| NLCN23WW | `ce531517b25d8af42aaaff8efe342a45624eab31b4a8d65d75d89b7534daaeca` | no | — |
| NLCN27WW | `73a393bd6a330291c8d9ee80e5e8e776d3c7e4ea164b6a2d39ed2901f3ea060d` | no | — |
| NLCN30WW | `b3711549e96961acc12fbb8451aa27d896107b706feb76bce4b32141f20d52a6` | no | — |
| NLCN33WW | `289846dcb9bd0cf33f1cdac363a2cb68024e44a857b16f6474366eaaed31db8d` | no | — |
| NLCN35WW | `289846dcb9bd0cf33f1cdac363a2cb68024e44a857b16f6474366eaaed31db8d` | no | — |
| NLCN36WW | `289846dcb9bd0cf33f1cdac363a2cb68024e44a857b16f6474366eaaed31db8d` | no | — |
| **NLCN37WW** | `2c36d96f8d6d20aa9f65084eb3cfe33b683fbf777439fc4a5954411d12d72502` | **YES** | `0x3170c` |
| **NLCN38WW** | `2c36d96f8d6d20aa9f65084eb3cfe33b683fbf777439fc4a5954411d12d72502` | **YES** | `0x3170c` |
| **NLCN39WW** | `2c36d96f8d6d20aa9f65084eb3cfe33b683fbf777439fc4a5954411d12d72502` | **YES** | `0x3170c` |

## Reading the table

- **The regression boundary is NLCN36 → NLCN37.** Builds up to NLCN36 have no
  "hide flag = 0" instruction in the sorting routine, so the Advanced tab was
  reachable. NLCN37 introduced it.
- **NLCN37/38/39 ship a byte-identical `H2OFormBrowserDxe`** (same SHA256), so
  the same patch — and the same exact 9-byte fallback anchor
  `ff 50 48 80 7b 10 00 74 de` — applies to all three. The released
  `BiosAdvancedPatch.efi` (v5) is a single binary that covers all three; there
  is no per-version build.
- **NLCN38 and NLCN39 were cross-checked at the module level** and are identical
  in every place that matters to this patch:

  | Module | NLCN38 SHA256 (16) | NLCN39 SHA256 (16) | |
  |---|---|---|---|
  | `H2OFormBrowserDxe` (PE32) | `2c36d96f8d6d20aa` | `2c36d96f8d6d20aa` | identical |
  | `SetupUtilityApp` (PE32) | `a62eb87d9fcdd5fc` | `a62eb87d9fcdd5fc` | identical |

  The Advanced FormSet (`C6D4769E-…`) is present in the NLCN39 Setup IFR with the
  same GUID, and the `EFI_FORM_BROWSER2_PROTOCOL` GUID the patcher uses to
  self-locate is present in the driver. The patch was confirmed on hardware on
  NLCN38WW; NLCN39WW is covered by this byte-level identity.
- Identical SHA256 across NLCN33/35/36 (and across NLCN22/23) just means the
  driver was not rebuilt between those releases.

## Other models / future versions

The table covers the G6+ AHP `NLCN` line. For a different model or a newer build
not listed here, do not assume the offset or bytes — run
[`analyze_formbrowser.py`](03-reproduce-analysis.md) on that firmware's
`H2OFormBrowserDxe`. If it reports a filter site, the runtime patcher's
structural matcher will handle it; if it reports none, that build does not hide
Advanced this way.
