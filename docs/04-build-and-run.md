# 4. Build and run

## The two files you need on the USB stick

1. **`BiosAdvancedPatch.efi`** — this project's runtime patcher
   (`patcher/prebuilt/BiosAdvancedPatch.efi`, or build it — below).
2. **`SetupUtilityApp.efi`** — *your laptop's own* Setup application, extracted
   from its firmware. This project does **not** ship it (it is Lenovo's
   copyrighted binary). Extract it from your BIOS image with UEFITool: it is the
   `SetupUtilityApp` DXE application's PE32 section. Save it to the stick under
   any name, e.g. `SetupUtilityApp.efi`.

Both files go on a **FAT32** USB stick (root or any folder).

## Prebuilt binary

```
patcher/prebuilt/BiosAdvancedPatch.efi
SHA256: eb0c1d6e4ded03db4a86bcd26f1cddd5be274ccab31331d1473211d7574346e6
```

This is a reproducible build (PE timestamp zeroed); rebuilding in the Nix dev
shell yields the same hash. One binary covers **NLCN37/38/39** — those builds
ship a byte-identical form browser, so there is no per-version binary (verified
on NLCN38WW and NLCN39WW).

## Building from source

```bash
cd patcher
nix develop ..        # or `nix develop` from the repo root, then cd patcher
make
sha256sum BiosAdvancedPatch.efi
```

Requirements (all in `flake.nix`): `gnu-efi`, GNU `binutils` (`ld`, `objcopy`),
`gcc`. The `Makefile` compiles for the SysV ABI and lets the gnu-efi
`crt0-efi-x86_64.o` translate the UEFI (MS-ABI) entry point — do not add
`-mabi=ms`. `.rodata` is explicitly copied into the PE (`objcopy -j .rodata`);
omitting it produces an app that prints garbage.

## Running it

You need the **UEFI Shell**. Either your firmware has one built in (look for a
"UEFI Shell" or "EFI Shell" boot entry / in the boot menu), or put a
`shellx64.efi` on the stick and boot it.

In the shell:

```
Shell> fs0:                       # switch to the USB stick (could be fs1:, fs2:, …)
FS0:\> connect -r                 # bind all drivers so the form browser is loaded
FS0:\> BiosAdvancedPatch.efi      # apply the runtime patch
FS0:\> SetupUtilityApp.efi        # launch the stock Setup — Advanced tab now present
```

`connect -r` matters: the patch targets the *resident* form-browser driver, so
that driver must already be loaded. From a cold boot into the shell it usually
is, but `connect -r` guarantees it.

## What success looks like

`BiosAdvancedPatch.efi` prints something like:

```
=== BiosAdvancedPatch v5 ===
Verified: ThinkBook G6+ AHP  NLCN37/38/39 (same form browser).
Locating the form browser and disabling the Advanced-tab filter.

FormBrowser2 at 0x...., SendForm=0x....
Form browser image: base=0x.... size=0x....
[site] +0x03170c : cmp [reg+10],0 ; 74 de (jcc)
       after: 90 90  -> OK (verified)

Structural sites: 1   patched OK: 1

[DONE] Advanced filter disabled for this boot.
       Now run SetupUtilityApp; the Advanced tab will appear.
```

The **`after: 90 90 -> OK (verified)`** line is the proof the write physically
landed (the app reads the bytes back). Then `SetupUtilityApp.efi` opens Setup
with the Advanced tab visible next to Main/Security/Boot.

## Troubleshooting — read the app's output

| What it prints | Meaning / action |
|---|---|
| `after: 90 90 -> OK` **and** Advanced shows | Success. |
| `FormBrowser2 not found` | The browser isn't loaded. Run `connect -r` first, or boot the shell from cold. |
| `Structural sites: 0` then fallback finds nothing | This build has no filter (Advanced may already be visible), or it is a model the idiom doesn't match — run `analyze_formbrowser.py` on its `H2OFormBrowserDxe` ([doc 3](03-reproduce-analysis.md)). |
| `after: 74 de -> FAILED (write-protected)` | The firmware blocked the write even past the memory-attribute remap and `CR0.WP` bypass. Report it; a different injection point would be needed. |
| `OK (verified)` but Advanced still absent | The byte patch is correct but the filter is reached another way on this build — capture the output and the firmware version for deeper tracing. |

## Notes

- The patch is **volatile**: it is gone after any reboot/reset. Re-run
  `BiosAdvancedPatch.efi` each time you want the Advanced tab.
- It writes only to the in-RAM copy of the driver. Flash is never touched.
- `SetupUtilityApp` stays **unmodified**; `SendForm` is still called with a zero
  FormSetGuid — the normal code path.
