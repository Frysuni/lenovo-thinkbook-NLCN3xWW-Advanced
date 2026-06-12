#!/usr/bin/env bash
#
# make-release.sh — publish a GitHub release with the prebuilt patcher attached.
#
# Requires `gh` authenticated (run `gh auth login` once, or set GH_TOKEN).
# The repo must already have an 'origin' remote on GitHub.
#
# Usage:
#   scripts/make-release.sh [tag]
#
# Default tag: v5.0
#
# It creates one release carrying BiosAdvancedPatch.efi (which covers
# NLCN37/38/39) and the notes from RELEASE_NOTES.md. It also pushes per-version
# tags (NLCN37WW/38WW/39WW) pointing at the same commit, so people searching by
# their BIOS version land on the right place.
#
# NOTE: SetupUtilityApp.efi is Lenovo's copyrighted binary and is deliberately
# NOT attached. Users extract it with scripts/extract-from-bios.sh.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

TAG="${1:-v5.0}"
EFI="patcher/prebuilt/BiosAdvancedPatch.efi"
NOTES="RELEASE_NOTES.md"
VERSIONS=(NLCN37WW NLCN38WW NLCN39WW)

command -v gh >/dev/null 2>&1 || { echo "error: gh not found. Run inside 'nix develop'." >&2; exit 1; }
[ -f "$EFI" ]   || { echo "error: missing $EFI (build it first)." >&2; exit 1; }
[ -f "$NOTES" ] || { echo "error: missing $NOTES." >&2; exit 1; }

gh auth status >/dev/null 2>&1 || { echo "error: gh not authenticated. Run 'gh auth login' or set GH_TOKEN." >&2; exit 1; }

echo "==> sha256 of asset:"
sha256sum "$EFI"

# Per-version tags (idempotent) so each BIOS version has a named ref.
for v in "${VERSIONS[@]}"; do
    if ! git rev-parse "$v" >/dev/null 2>&1; then
        git tag -a "$v" -m "Covered by BiosAdvancedPatch ($v: byte-identical form browser)"
    fi
done
git push origin --tags

echo "==> creating release $TAG"
if gh release view "$TAG" >/dev/null 2>&1; then
    gh release upload "$TAG" "$EFI" --clobber
    gh release edit "$TAG" --notes-file "$NOTES"
else
    gh release create "$TAG" "$EFI" \
        --title "BiosAdvancedPatch $TAG — ThinkBook Advanced menu (NLCN37/38/39)" \
        --notes-file "$NOTES"
fi

echo "==> done. Release $TAG published with $(basename "$EFI")."
echo "    Per-version tags pushed: ${VERSIONS[*]}"
