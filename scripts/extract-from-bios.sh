#!/usr/bin/env bash
#
# extract-from-bios.sh — pull the two files this project needs out of a Lenovo
# ThinkBook BIOS package:
#
#   * SetupUtilityApp.efi      — the stock Setup app you launch after patching
#                                (Lenovo's binary; we do NOT redistribute it,
#                                 you extract it from your own firmware here)
#   * H2OFormBrowserDxe.efi     — the form-browser driver, for analysis
#
# It also runs analyze_formbrowser.py on the extracted driver so you can see
# immediately whether your build hides the Advanced tab and what the patch is.
#
# Usage:
#   scripts/extract-from-bios.sh <BIOS-NLCNxxWW.exe | image.bin> [outdir]
#
# Default outdir: ./out
#
# Requires the Nix dev shell (`nix develop`): provides p7zip, uefitool, python3,
# capstone. Run this script from inside `nix develop`, or it will try to pull
# the tools via `nix run`.

set -euo pipefail

# --- module identifiers -----------------------------------------------------
FORMBROWSER_GUID="9E5DAEB4-4B91-4466-9EBE-81C7E4401E6D"
FORMBROWSER_NAME="H2OFormBrowserDxe"
SETUP_NAME="SetupUtilityApp"

# --- locate tools (prefer PATH from the dev shell, else nix run) ------------
have() { command -v "$1" >/dev/null 2>&1; }
SEVENZ="7z";          have 7z          || SEVENZ="nix run nixpkgs#p7zip --"
UEFIEXTRACT="uefiextract"; have uefiextract || UEFIEXTRACT="nix run nixpkgs#uefitool --"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ANALYZER="$SCRIPT_DIR/../patcher/analyze_formbrowser.py"

# --- args -------------------------------------------------------------------
if [ $# -lt 1 ]; then
    echo "usage: $0 <BIOS-NLCNxxWW.exe | image.bin> [outdir]" >&2
    exit 2
fi
INPUT="$1"
OUTDIR="${2:-./out}"
[ -f "$INPUT" ] || { echo "error: no such file: $INPUT" >&2; exit 1; }
mkdir -p "$OUTDIR"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

echo "==> input : $INPUT"
echo "==> outdir: $OUTDIR"

# --- step 1: get the raw SPI image -----------------------------------------
case "$INPUT" in
    *.bin|*.BIN|*.fd|*.FD|*.rom|*.ROM|*.cap|*.CAP)
        IMG="$INPUT"
        echo "==> using image directly"
        ;;
    *)
        echo "==> extracting SPI image from package with 7z..."
        $SEVENZ e -y -o"$WORK" "$INPUT" >/dev/null 2>&1 || true
        IMG="$(find "$WORK" -maxdepth 1 -type f \( -iname '*.bin' -o -iname '*.fd' \
                 -o -iname '*.rom' -o -iname '*.cap' \) -printf '%s %p\n' \
               | sort -nr | head -1 | cut -d' ' -f2-)"
        [ -n "$IMG" ] || { echo "error: no firmware image inside $INPUT" >&2; exit 1; }
        echo "    image: $(basename "$IMG") ($(stat -c%s "$IMG") bytes)"
        ;;
esac

# --- step 2: explode the firmware volume ------------------------------------
echo "==> running uefiextract..."
cp "$IMG" "$WORK/fw.bin"
$UEFIEXTRACT "$WORK/fw.bin" >/dev/null 2>&1 || true
DUMP="$WORK/fw.bin.dump"
[ -d "$DUMP" ] || { echo "error: uefiextract produced no dump" >&2; exit 1; }

# --- step 3: pull the two PE32 modules --------------------------------------
extract_pe32() {  # <name-fragment> <output-file>
    local frag="$1" out="$2" src
    src="$(find "$DUMP" -path "*${frag}*PE32*body.bin" 2>/dev/null | head -1)"
    if [ -z "$src" ]; then
        echo "    WARNING: $frag PE32 not found" >&2
        return 1
    fi
    cp "$src" "$out"
    echo "    $out  ($(stat -c%s "$out") bytes)  sha256=$(sha256sum "$out" | cut -c1-16)…"
}

echo "==> extracting modules:"
extract_pe32 "$SETUP_NAME"       "$OUTDIR/SetupUtilityApp.efi"      || true
extract_pe32 "$FORMBROWSER_NAME" "$OUTDIR/H2OFormBrowserDxe.efi"    || true

# --- step 4: analyze the form browser ---------------------------------------
if [ -f "$OUTDIR/H2OFormBrowserDxe.efi" ]; then
    echo ""
    echo "==> analyzing form browser for the Advanced-tab filter:"
    python3 "$ANALYZER" "$OUTDIR/H2OFormBrowserDxe.efi" || true
fi

echo ""
echo "Done."
echo "  Copy $OUTDIR/SetupUtilityApp.efi and patcher/prebuilt/BiosAdvancedPatch.efi"
echo "  onto a FAT32 USB stick and follow docs/04-build-and-run.md."
