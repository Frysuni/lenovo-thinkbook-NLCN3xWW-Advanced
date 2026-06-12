# Contributing

Thanks for helping extend this. The most valuable contributions are **adding or
confirming support for more BIOS versions and ThinkBook models**, and improving
the analysis tooling. No Lenovo/Insyde firmware is ever committed here — only
original code, tooling, and analysis notes.

## Dev environment

Everything runs in the Nix dev shell:

```bash
nix develop
```

It provides the full toolchain: `gnu-efi` + `gcc` (build the patcher),
`capstone`/`python3` (analyzer), `uefitool` + `p7zip` (extraction), `ghidra`
(decompilation), `radare2`/`binutils`, and `gh` (releases).

Repo layout:

```
patcher/        the UEFI patcher (C + Makefile) and the analyzer
  prebuilt/     committed, reproducible BiosAdvancedPatch.efi
scripts/        extract-from-bios.sh, decompile.sh, make-release.sh
research/        Ghidra scripts + decompiled key functions
docs/            English docs;  docs/ru/  Russian docs
```

## Add / verify support for a new BIOS version

1. **Get the firmware** for your model (see
   [docs/07-firmware-downloads.md](docs/07-firmware-downloads.md)). Never commit
   it — it is gitignored.

2. **Extract and analyze** in one step:

   ```bash
   scripts/extract-from-bios.sh BIOS-NLCNxxWW.exe out/
   ```

   This writes `out/SetupUtilityApp.efi` and `out/H2OFormBrowserDxe.efi`, then
   runs the analyzer. Read its verdict:

   - **`filter PRESENT (patchable)`** with one site → this build hides Advanced
     the documented way. Note the offset and the 9-byte anchor.
   - **`no filter found`** → this build does not hide Advanced this way.

3. **Record the result** in [docs/05-cross-version-matrix.md](docs/05-cross-version-matrix.md)
   (and `docs/ru/05-…`): add a row with the version, the `H2OFormBrowserDxe`
   PE32 SHA256, whether the filter is present, and the offset. Get the hash with:

   ```bash
   sha256sum out/H2OFormBrowserDxe.efi
   ```

4. **If the bytes differ from the known anchor** (different register or jump
   displacement), the runtime patcher's *structural* matcher should still handle
   it — but say so in your PR, and ideally test on hardware. If the idiom itself
   is absent but Advanced is still hidden, that is a new variant: capture the
   decompilation (next section) and open an issue.

5. **Confirm on hardware** if you can, and report the patcher's on-screen output
   (`after: 90 90 -> OK` etc.). Hardware confirmation is the gold standard;
   byte-identity to an already-verified build is the silver standard (that is how
   NLCN39 is covered — see the matrix).

## Decompiling for deeper analysis

```bash
# Decompile the filter and its caller from an extracted module:
scripts/decompile.sh out/H2OFormBrowserDxe.efi 0x314f4 0x1018 0x1fec
```

The Ghidra headless scripts live in [research/ghidra-scripts/](research/ghidra-scripts/);
the annotated reference decompilation and an explanation of the three relevant
functions are in [research/README.md](research/README.md).

## Changing the patcher

- Source: [patcher/BiosAdvancedPatch.c](patcher/BiosAdvancedPatch.c). Keep the
  SysV-ABI build assumptions in [patcher/Makefile](patcher/Makefile) (no
  `-mabi=ms`; `.rodata` must be copied). Read the comments in both.
- The build is **reproducible** (PE timestamp zeroed). After changing the
  source, rebuild and refresh the prebuilt + its SHA256 in the docs:

  ```bash
  cd patcher && make && sha256sum BiosAdvancedPatch.efi
  cp BiosAdvancedPatch.efi prebuilt/ && make clean
  ```

  Update the hash in `docs/04-build-and-run.md` and `docs/ru/04-…`.
- Bump the `vN` string in the banner when behaviour changes.

## Documentation

Docs are bilingual: English in `docs/`, Russian in `docs/ru/`, with mirrored
filenames. If you change one language, update the other (or note in your PR that
a translation is needed). The READMEs (`README.md` RU, `README.en.md` EN) link
both ways.

## Pull requests

- Keep firmware and large dumps out of the diff (they are gitignored; double
  check with `git status`).
- For a new version: matrix row + a line in the PR describing how you verified.
- For tooling/patcher changes: note how you tested, and update the prebuilt hash
  if the binary changed.

## Reporting problems

Open an issue with: your exact BIOS version (e.g. `NLCN3xWW`), model/type number,
the analyzer output for your `H2OFormBrowserDxe`, and — if you ran it — the
on-screen output of `BiosAdvancedPatch.efi`.
