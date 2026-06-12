# research/

Reverse-engineering material behind the patch. None of this is needed to *use*
the tool — it is here so the analysis is reproducible and reviewable, and so
contributors can extend it to new firmware.

## Contents

```
research/
├── ghidra-scripts/        Ghidra headless scripts (used by scripts/decompile.sh)
│   ├── DecompileFunctions.java     decompile functions at given addresses
│   ├── FindGuidXrefsDecompile.java find code that references a GUID
│   └── FindStringXrefsDecompile.java find code that references a string
└── decompiled/
    └── H2OFormBrowserDxe_NLCN37-39_key-functions.txt
                            Ghidra decompilation of the three functions that
                            implement and reach the Advanced-tab filter.
```

The decompiled `.txt` was produced from the NLCN38WW `H2OFormBrowserDxe` PE32
section (sha256 `2c36d96f8d6d20aa9f65084eb3cfe33b683fbf777439fc4a5954411d12d72502`,
identical in NLCN37/38/39). Regenerate it for any module with:

```bash
scripts/decompile.sh out/H2OFormBrowserDxe.efi 0x314f4 0x1018 0x1fec
```

## The three functions and how they relate to the patch

### `FUN_000314f4` — the sort/filter (this is what we patch)

The browser's FormSet sorting routine. It walks the internal table at
`DAT_00037c30` (= file offset `0x37c30`) where each entry is a 16-byte FormSet
GUID followed by a flag. For each input HII handle it finds the matching table
entry, then:

```c
/* plVar10 points at the matched table entry; plVar10[2] is the flag byte. */
if ((char)plVar10[2] == '\0') goto LAB_000316f0;   /*  <-- THE FILTER  */
...                                                /* flag!=0: keep/sort it */
LAB_000316f0:
    uVar11 = uVar11 + 1;                            /* flag==0: skip, next handle */
```

That `if ((char)plVar10[2] == '\0') goto LAB_000316f0;` compiles to

```
0x3170c:  cmp byte ptr [rbx+0x10], 0
0x31710:  jz  0x316f0
```

The Advanced FormSet's flag is `0`, so it always takes the skip. **The patch
NOPs the `jz` at `0x31710` (`74 de → 90 90`)** so flag-0 FormSets are no longer
dropped. See [docs/02-root-cause.md](../docs/02-root-cause.md).

### `FUN_00001018` — the `SendForm` worker that calls the filter

```c
ppuVar3 = FUN_00001fec();              /* is the sort context active? */
if (ppuVar3 != (undefined **)0x0) {
    FUN_000314f4(puVar1,&local_res18,(ulonglong)puVar7);   /* sort + FILTER */
    uVar6 = local_res18;               /* surviving handle count */
}
```

This is on the normal `EFI_FORM_BROWSER2_PROTOCOL.SendForm` path the stock
`SetupUtilityApp` uses. The handle list it receives already includes the
Advanced FormSet; `FUN_000314f4` is where it gets removed.

### `FUN_00001fec` — the gate

Returns non-NULL when the sort context is present, which is what makes
`FUN_00001018` call the filter. Documented here for completeness; the patch does
not touch it.

## Reproducing the wider analysis

- Extract modules from a BIOS package: [`scripts/extract-from-bios.sh`](../scripts/extract-from-bios.sh).
- Locate/verify the filter in any module: [`patcher/analyze_formbrowser.py`](../patcher/analyze_formbrowser.py).
- Decompile arbitrary functions: [`scripts/decompile.sh`](../scripts/decompile.sh).
- Full method writeups: [docs/02-root-cause.md](../docs/02-root-cause.md) and
  [docs/03-reproduce-analysis.md](../docs/03-reproduce-analysis.md).
