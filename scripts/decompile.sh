#!/usr/bin/env bash
#
# decompile.sh — Ghidra headless decompilation of a UEFI module.
#
# Imports a PE32/TE binary, runs auto-analysis, and decompiles the functions at
# the addresses you give (addresses are relative to the module's image base,
# which for these PE32 sections is 0 — i.e. the same file offsets the analyzer
# and docs use).
#
# Usage:
#   scripts/decompile.sh <module.efi|body.bin> <addr> [addr...]
#
# Example — decompile the Advanced-tab filter and its caller in H2OFormBrowserDxe:
#   scripts/decompile.sh out/H2OFormBrowserDxe.efi 0x314f4 0x1018
#
# Output goes to ./decompiled-<module>.txt and is also printed.
#
# Requires the Nix dev shell (`nix develop`) for `ghidra-analyzeHeadless`.

set -euo pipefail

if [ $# -lt 2 ]; then
    echo "usage: $0 <module> <addr> [addr...]" >&2
    exit 2
fi

MODULE="$1"; shift
[ -f "$MODULE" ] || { echo "error: no such file: $MODULE" >&2; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GHIDRA_SCRIPTS="$SCRIPT_DIR/../research/ghidra-scripts"
OUT="./decompiled-$(basename "$MODULE").txt"

have() { command -v "$1" >/dev/null 2>&1; }
HEADLESS="ghidra-analyzeHeadless"
have "$HEADLESS" || HEADLESS="nix run nixpkgs#ghidra -- ghidra-analyzeHeadless"

PROJ="$(mktemp -d)"
trap 'rm -rf "$PROJ"' EXIT

echo "==> importing $MODULE into a temporary Ghidra project and analyzing..."
$HEADLESS "$PROJ" tmp \
    -import "$MODULE" \
    -scriptPath "$GHIDRA_SCRIPTS" \
    -postScript DecompileFunctions.java "$OUT" "$@" \
    -deleteProject >/dev/null 2>&1 || true

if [ -f "$OUT" ]; then
    echo "==> wrote $OUT"
    echo ""
    cat "$OUT"
else
    echo "error: Ghidra produced no output (see above)." >&2
    exit 1
fi
