# 6. Safety and limitations

## What this tool does

- Locates the **already-running** `H2OFormBrowserDxe` in RAM.
- Replaces **one 2-byte conditional jump** with two `NOP`s.
- Reads the bytes back to confirm the change.
- Nothing else.

## What it does NOT do

- It does **not** write the SPI flash. The firmware on the chip is untouched.
- It does **not** modify NVRAM/UEFI variables.
- It does **not** replace, patch, or wrap `SetupUtilityApp` — you launch the
  stock Lenovo binary.
- It does **not** change the FormSet flag table or the `SendForm` arguments.
- It does **not** persist. Any reboot, warm reset, or power cycle reverts it.

Because the only change is to a volatile in-memory copy of a driver, **this
patch cannot brick the machine**. Worst case, the patch fails to apply (it tells
you so) or the app exits — a reset returns everything to stock.

## The real risk is what you do in Advanced

Unlocking the Advanced/CBS menus exposes low-level settings (memory timings,
SoC/CBS knobs, chipset and debug options). **Changing those can make the system
fail to POST or boot.** That risk comes from the settings you change, not from
this patch. Recommendations:

- Note every value before you change it.
- Change one thing at a time.
- Know how to clear CMOS / reset BIOS defaults for your model before you start.
- Settings you change are stored in NVRAM and **do** persist across reboots,
  even though the unlock itself does not.

## Limitations

- **Volatile:** re-run `BiosAdvancedPatch.efi` after every boot to see Advanced.
- **Requires the UEFI Shell** and the ability to run unsigned EFI apps. With
  Secure Boot enabled you will generally need to disable it (or enrol the app),
  since neither this app nor a stock shell is signed by Lenovo.
- **Write protection:** if the firmware enforces strong code-page protection
  that survives both the `EFI_MEMORY_ATTRIBUTE_PROTOCOL` remap and the `CR0.WP`
  bypass, the write will fail — the app reports `FAILED (write-protected)` and
  changes nothing. Confirmed working on NLCN38WW; other platforms may differ.
- **Model/version scope:** verified on ThinkBook 16 G6+ AHP (NLCN38WW). Other
  models that share the Insyde/Byosoft form browser are likely compatible but
  must be checked with [`analyze_formbrowser.py`](03-reproduce-analysis.md)
  first.

## Secure Boot and signing

The patcher is an unsigned UEFI application. If your machine has Secure Boot on,
either disable it for the session, or sign the app and enrol your key. Running
unsigned EFI binaries is your decision and responsibility.

## Disclaimer

Provided as-is under the [MIT License](../LICENSE), with no warranty. You are
responsible for understanding what you run on your own hardware and for any
configuration changes you make once the Advanced menu is unlocked.
