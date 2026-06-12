# 2. Root cause

All offsets below are **file offsets inside the decompressed PE32 section** of
`H2OFormBrowserDxe` (module GUID `9E5DAEB4-4B91-4466-9EBE-81C7E4401E6D`), as
shipped in NLCN37/38/39 (those three are byte-identical:
`sha256 2c36d96f8d6d20aa9f65084eb3cfe33b683fbf777439fc4a5954411d12d72502`).

## The filter instruction

`H2OFormBrowserDxe` sorts and filters the list of HII FormSet handles before
the browser renders tabs. Its sorting routine (Ghidra `FUN_000314f4`, base
`0x314f4`) contains this sequence:

```
offset    bytes              disassembly
--------  -----------------  -------------------------------------------
0x31709   ff 50 48           call qword ptr [rax+0x48]    ; FreePool
0x3170c   80 7b 10 00        cmp  byte ptr [rbx+0x10], 0  ; the FormSet's flag
0x31710   74 de              jz   0x316f0                 ; flag==0 -> skip it
```

`[rbx+0x10]` is a one-byte "show this FormSet" flag taken from an internal
table. When it is `0`, the `jz` branches back into the loop **without adding the
FormSet to the output list** — the FormSet is silently dropped.

## The flag table

The internal table lives at offset `0x37c30` (20-byte entries: 16-byte FormSet
GUID + 4-byte flag). The relevant rows:

| FormSet | GUID | flag |
|---|---|---|
| Debug | `A6E38A2F-…` | 0 |
| Main | `72E7C0DF-…` | 1 |
| Security | `7A3A87F5-…` | 1 |
| Boot | `7B3A3A54-…` | 1 |
| (chipset) | `C1E0B01A-…` | 0 |
| **Advanced** | **`C6D4769E-7F48-4D2A-98E9-87ADCCF35CCC`** | **0** |
| (misc) | `5204F764-…` | 1 |
| Power | `A6712873-…` | 0 |
| (misc) | `2D068309-…` | 1 |
| (misc) | `B863B959-…` | 0 |
| CBS | `BFB68FED-…` | 0 |
| (misc) | `B6936426-…` | 1 |

The Advanced FormSet's flag is `0`, so the `jz` at `0x31710` removes it from
every browser session.

**Key finding:** the flag value did **not** change between firmware versions —
Advanced was always `0`. What changed is that the *filter instruction itself*
was added. Pre-NLCN37 builds have no `cmp byte [reg+0x10],0 ; jz` in the
sorting routine, so flag = 0 FormSets were never skipped and Advanced was
visible. See [the version matrix](05-cross-version-matrix.md) for the exact
boundary (NLCN36 → NLCN37).

## The call path (why the stock Setup app hits this)

`SetupUtilityApp` builds its tab list and calls the browser the normal way:

```
SetupUtilityApp
  -> EFI_FORM_BROWSER2_PROTOCOL.SendForm(handles, count, FormSetGuid=NULL, FormId=1, …)
       -> H2OFormBrowserDxe internal SendForm (FUN_00001018)
            -> FUN_000314f4   ; sort + FILTER  <-- drops flag==0 FormSets here
            -> build page list from the surviving handles
            -> render tabs
```

`SendForm` is called with a **NULL/zero FormSetGuid** (meaning "show all the
handles I gave you"), which is the stock behaviour. The handle list that
`SetupUtilityApp` passes *does* include the Advanced FormSet's HII handle — it
is `FUN_000314f4` inside the browser that throws it away. That is why patching
`SetupUtilityApp` (reordering or trimming its handle list) never worked: the
filter runs afterwards, inside the browser, regardless.

## The fix

Replace the 2-byte conditional jump with two `NOP`s:

```
offset 0x31710:   74 de   (jz 0x316f0)      ->   90 90   (nop; nop)
```

Now `cmp byte [rbx+0x10],0` still executes but its result is ignored; control
falls through and **every** FormSet — including Advanced (and CBS, Debug, etc.)
— is added to the list. This reproduces exactly the pre-NLCN37 behaviour.

Nothing else is touched: the flag table is unchanged, `SendForm` is still
called with a zero FormSetGuid, the handle list is unchanged, and
`SetupUtilityApp` itself is byte-for-byte the stock Lenovo binary.

## Why a generic, not byte-fixed, detector

The exact bytes (`ff 50 48 80 7b 10 00 74 de`) are specific to this compiler
output. On a different build the register (`rbx`), the jump displacement
(`0xde`), or the preceding instruction may differ. The semantically invariant
part is the **idiom**:

```
cmp byte ptr [<any base reg>+0x10], 0   ;  80 /7 [mod=01] 10 00
j(n)z <backward>                        ;  74|75 <negative rel8>
```

Both the runtime patcher and the offline analyzer match this idiom structurally,
so they keep working across versions and across similar models that share the
form-browser code. See [reproduce the analysis](03-reproduce-analysis.md).
