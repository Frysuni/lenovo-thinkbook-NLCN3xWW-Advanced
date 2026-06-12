# 3. Reproduce the analysis (for any firmware / model)

This is how to confirm — from scratch — that a given BIOS hides the Advanced tab
the way described in [root cause](02-root-cause.md), and to derive the exact
patch for a build that is not in [the matrix](05-cross-version-matrix.md).

Everything here runs in the Nix dev shell:

```bash
nix develop          # provides UEFITool, capstone, python3, radare2, binutils, …
```

## Step 1 — get the firmware image

Download the BIOS package for your model (see
[firmware downloads](07-firmware-downloads.md)) and obtain the raw SPI image.
The Lenovo `NLCN` packages are Insyde `H2OFFT` installers; the flashable image
is the large `.bin` /`.FD`/`.cap` inside. If you already have a full SPI dump,
use that.

## Step 2 — extract `H2OFormBrowserDxe`

The filter only exists in the **decompressed** driver. You cannot find it by
scanning the raw `.bin` (the driver lives inside a compressed firmware volume).

Use UEFITool (GUI) or `uefiextract` (CLI, in the dev shell):

```bash
uefiextract NLCN38WW.bin            # produces NLCN38WW.bin.dump/
```

Then locate the module by GUID `9E5DAEB4-4B91-4466-9EBE-81C7E4401E6D` and take
its **"PE32 image section" → `body.bin`**. In the dump tree it is:

```
…/153 H2OFormBrowserDxe/2 PE32 image section/body.bin
```

> Tip: `find NLCN38WW.bin.dump -path '*H2OFormBrowserDxe*PE32*body.bin'`

## Step 3 — locate the filter and derive the patch

```bash
python3 patcher/analyze_formbrowser.py \
    'NLCN38WW.bin.dump/…/153 H2OFormBrowserDxe/2 PE32 image section/body.bin'
```

Example output:

```
=== …/body.bin  (244512 bytes) ===
  Found 1 filter site(s):

  [filter @ file offset 0x03170c]
    cmp byte [rbx+0x10], 0 ; jz -34
    0x03170c: cmp     byte ptr [rbx + 0x10], 0  <== filter
    0x031710: je      0x3170c... (backward)
    0x031712: mov     rcx, qword ptr [rbx]
    …
    search anchor (9 bytes) : ff 50 48 80 7b 10 00 74 de
    patch the J-byte at off : 0x031710  (74 de  ->  90 90)

Summary: filter PRESENT (patchable)
```

Interpretation:

- **`Found N filter site(s)`** → this build hides flag = 0 FormSets. Patch the
  J-byte(s) it reports (`74/75 xx → 90 90`).
- **`No 'flag==0 -> skip' filter found`** → this build does **not** hide
  Advanced; nothing to patch (it should already be visible).

The tool matches the idiom *structurally* (`cmp byte [reg+0x10],0 ; j(n)z
backward`), so it tolerates a different register or jump displacement on other
builds. If it reports **more than one** site, disassemble around each (the tool
prints a window) and patch the one inside the FormSet-sorting routine.

## Step 4 — (optional) confirm the flag table

To independently confirm that Advanced is flagged "hide", inspect the table at
the offset the analysis describes (`0x37c30` on NLCN38). Each entry is a 16-byte
FormSet GUID followed by a 4-byte flag; the Advanced GUID
`C6D4769E-7F48-4D2A-98E9-87ADCCF35CCC` carries flag `0`. You can also dump the
Setup IFR with the **Universal IFR Extractor** (external, optional) to see the
Advanced FormSet definition:

```
Form Set: Advanced [C6D4769E-7F48-4D2A-98E9-87ADCCF35CCC], …
  VarStore: … Name: SystemConfig
  Form: Advanced, FormId: 0x1
```

## Step 5 — confirm the runtime patcher will hit it

The runtime `BiosAdvancedPatch.efi` uses the same idiom matcher as the offline
tool, confined to the form-browser image it locates at runtime. If the offline
tool reports exactly one site for your build, the runtime patcher will find and
NOP it (and it prints the before/after bytes so you can verify on the machine).

## External tools used

- **UEFITool / uefiextract** — firmware volume extraction. Provided by the Nix
  dev shell (`pkgs.uefitool`).
- **capstone** — disassembly used by `analyze_formbrowser.py`. In the dev shell.
- **Universal IFR Extractor** (optional) — human-readable FormSet/IFR dumps.
  Not required for the patch; fetch from its upstream project if you want it.
