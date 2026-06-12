#!/usr/bin/env python3
"""
analyze_formbrowser.py — locate (and emit the patch for) the "Advanced tab"
filter inside Insyde/Byosoft H2OFormBrowserDxe.

Background
----------
On affected firmware the form browser's FormSet-sorting routine contains an
instruction idiom that drops any FormSet whose internal "show" flag byte is 0:

        cmp  byte ptr [<reg>+0x10], 0      ; read the per-FormSet flag
        jz   <backward>                    ; flag==0 -> skip this FormSet

The Advanced FormSet ships with flag=0, so it is silently removed from every
SendForm() call. Older firmware did not have this instruction, so Advanced was
visible. NOP-ing the conditional jump (74 xx -> 90 90) makes every FormSet pass,
restoring the Advanced tab.

This script finds that idiom in a binary so the exact patch can be derived and
verified for *any* version, not just the ones we hand-checked.

Usage
-----
    analyze_formbrowser.py <file> [<file> ...]

<file> may be:
  * an extracted H2OFormBrowserDxe PE32 section (body.bin), or
  * any blob (raw firmware volume / full BIOS .bin) — it is scanned globally.

Output: for every match, the file offset, the disassembled window (if capstone
is available), the 9-byte search anchor, and the 2-byte patch.

Exit code 0 if at least one filter site was found, 1 otherwise.
"""

import sys
import os

# capstone is optional; without it we still byte-match and report, just no asm.
try:
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64
    _MD = Cs(CS_ARCH_X86, CS_MODE_64)
    _MD.detail = False
except Exception:
    _MD = None

REGS = ["rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi"]


def find_filter_sites(data: bytes):
    """
    Return a list of dicts describing each 'cmp byte [reg+0x10],0 ; jz/jnz rel8'
    site whose conditional jump is *backward* (the loop-skip idiom).

    Match is structural, not a fixed byte string, so it survives register- and
    displacement-allocation changes between compiler versions.
    """
    sites = []
    n = len(data)
    for i in range(n - 6):
        # 0x80 /7 ib  = CMP r/m8, imm8   (reg field of ModRM == 7)
        if data[i] != 0x80:
            continue
        modrm = data[i + 1]
        mod = modrm >> 6
        reg = (modrm >> 3) & 7
        rm = modrm & 7
        if mod != 1 or reg != 7:          # mod=01 (disp8), /7 = CMP
            continue
        if rm == 4:                        # rm=100 would need a SIB byte
            continue
        if data[i + 2] != 0x10:            # disp8 == 0x10 (the flag offset)
            continue
        if data[i + 3] != 0x00:            # imm8 == 0
            continue
        jop = data[i + 4]
        if jop not in (0x74, 0x75):        # JZ / JNZ rel8
            continue
        jdisp = data[i + 5]
        signed = jdisp - 256 if jdisp >= 128 else jdisp
        if signed >= 0:                    # require a backward (skip) jump
            continue
        sites.append({
            "cmp_off": i,
            "reg": REGS[rm],
            "jop": jop,
            "jdisp": jdisp,
            "jname": "jz" if jop == 0x74 else "jnz",
            "signed": signed,
            "anchor_off": i - 3,           # 3 bytes of preceding context
            "anchor": data[i - 3:i + 6],   # 9-byte search anchor
            "jz_off": i + 4,               # byte offset of the 0x74/0x75
        })
    return sites


def disasm_window(data: bytes, start: int, nbytes=24):
    """Disassemble forward from `start` (the CMP) so decoding stays in sync."""
    if _MD is None:
        return ["    (capstone not installed — `nix develop` provides it)"]
    end = min(len(data), start + nbytes)
    out = []
    for insn in _MD.disasm(data[start:end], start):
        mark = "  <== filter" if insn.address == start else ""
        out.append(f"    0x{insn.address:06x}: {insn.mnemonic:<7} {insn.op_str}{mark}")
    return out


def analyze(path: str) -> int:
    with open(path, "rb") as f:
        data = f.read()
    print(f"\n=== {path}  ({len(data)} bytes) ===")
    sites = find_filter_sites(data)
    if not sites:
        print("  No 'flag==0 -> skip' filter found.")
        print("  -> This build does NOT hide the Advanced tab (nothing to patch).")
        return 1

    print(f"  Found {len(sites)} filter site(s):\n")
    for s in sites:
        print(f"  [filter @ file offset 0x{s['cmp_off']:06x}]")
        print(f"    cmp byte [{s['reg']}+0x10], 0 ; {s['jname']} {s['signed']:+d}")
        for line in disasm_window(data, s["cmp_off"]):
            print(line)
        anchor_hex = " ".join(f"{b:02x}" for b in s["anchor"])
        print(f"    search anchor (9 bytes) : {anchor_hex}")
        print(f"    patch the J-byte at off : 0x{s['jz_off']:06x}  "
              f"({s['jop']:02x} {s['jdisp']:02x}  ->  90 90)")
        print()
    return 0


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    rc = 1
    for path in argv[1:]:
        if not os.path.isfile(path):
            print(f"  skip (not a file): {path}", file=sys.stderr)
            continue
        if analyze(path) == 0:
            rc = 0
    print()
    print("Summary: " + ("filter PRESENT (patchable)" if rc == 0
                         else "no filter found in any input"))
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv))
