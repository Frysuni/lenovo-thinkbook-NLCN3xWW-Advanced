# 8. Getting into the UEFI Shell (Linux & Windows)

`BiosAdvancedPatch.efi` and your `SetupUtilityApp.efi` are run from the **UEFI
Shell**. This page shows how to get there from either OS.

> ThinkBook keys (G6+ AHP / 21LG): **F2** (or **Fn+F2**) enters BIOS Setup,
> **F12** (or **Fn+F12**) opens the one-time boot menu. The `Fn` modifier is
> needed if "Hotkey Mode / FnLock" makes the F-row act as media keys.

## 0. One-time prerequisites (both OSes)

1. **A FAT32 USB stick.** Put two files at its root:
   - `BiosAdvancedPatch.efi` (this project)
   - `SetupUtilityApp.efi` (extracted from *your* firmware — see
     [build & run](04-build-and-run.md))
2. **A UEFI Shell to boot.** Two options:
   - Many Insyde firmwares already provide one — look for a **"UEFI Shell"** or
     **"EFI Shell"** entry in the F12 boot menu. If it's there, you don't need
     to add anything.
   - Otherwise, add the standard shell to the stick: download `shellx64.efi`
     (the EDK2 UEFI Shell, e.g. from the tianocore/edk2 releases) and save it as
     **`\EFI\BOOT\BOOTX64.EFI`** on the stick. The firmware will offer the stick
     as a bootable UEFI device.
3. **Secure Boot** must usually be **off** to run these unsigned EFI files.
   In BIOS Setup (**F2**) → **Security → Secure Boot → Disabled**, save & exit.

## 1. From Windows

### Prepare the stick

- Format the USB as **FAT32** (right-click → Format, or `diskpart`).
- Copy `BiosAdvancedPatch.efi` and `SetupUtilityApp.efi` to the root.
- If you need the shell: copy `shellx64.efi` to `\EFI\BOOT\BOOTX64.EFI` on the
  stick (create the folders). [Rufus](https://rufus.ie) can also write a UEFI
  Shell image, but a plain FAT32 stick with `BOOTX64.EFI` is enough.

### Boot into it

Either:

- **Boot menu:** insert the stick, restart, and tap **F12** (or **Fn+F12**)
  during the Lenovo logo. Pick the USB stick / "UEFI Shell".
- **Windows "Advanced startup":** Settings → System → Recovery → *Restart now*
  under Advanced startup → **Use a device** → your USB stick. (Or run
  `shutdown /r /o /t 0` and choose *Use a device*.)

If Windows uses **BitLocker**, suspend it before changing BIOS/Secure Boot
settings to avoid a recovery-key prompt: `manage-bde -protectors -disable C:`.

## 2. From Linux

### Prepare the stick

```bash
# Identify the stick (e.g. /dev/sdX) with lsblk — be sure you pick the USB!
sudo mkfs.vfat -F32 /dev/sdX1            # format as FAT32 (erases it)
sudo mount /dev/sdX1 /mnt
sudo cp BiosAdvancedPatch.efi SetupUtilityApp.efi /mnt/
# Optional: add the shell so the stick itself is bootable
sudo mkdir -p /mnt/EFI/BOOT
sudo cp shellx64.efi /mnt/EFI/BOOT/BOOTX64.EFI
sudo umount /mnt
```

### Boot into it

- **Boot menu:** reboot and tap **F12** (or **Fn+F12**) at the Lenovo logo, then
  choose the USB stick / "UEFI Shell".
- **Or add a boot entry** with `efibootmgr` (advanced):
  ```bash
  sudo efibootmgr -c -d /dev/sdX -p 1 \
       -L "UEFI Shell" -l '\EFI\BOOT\BOOTX64.EFI'
  ```
  then select it from the firmware boot menu.

## 3. In the shell — find the stick and run

The shell lists file systems as `FS0:`, `FS1:`, … Map them and switch:

```
Shell> map -r                 # refresh / list filesystems
Shell> fs0:                   # try fs0; if it's not the stick, try fs1:, fs2: …
FS0:\> ls                     # confirm you see BiosAdvancedPatch.efi here
FS0:\> connect -r             # make sure the form browser driver is loaded
FS0:\> BiosAdvancedPatch.efi  # apply the runtime patch (watch for "OK (verified)")
FS0:\> SetupUtilityApp.efi    # launch stock Setup — Advanced tab is now present
```

If `fs0:` is the wrong device, run `ls` after each `fsN:` until you see
`BiosAdvancedPatch.efi`. See [build & run](04-build-and-run.md#troubleshooting--read-the-apps-output)
for what the patcher's output means.

## 4. Notes

- The patch is per-boot. After any reset you re-enter the shell and re-run
  `BiosAdvancedPatch.efi`.
- Re-enable Secure Boot afterwards if you want it on for normal use; the unlock
  itself leaves no persistent change (but any BIOS settings you alter in
  Advanced **do** persist — see [safety](06-safety-and-limitations.md)).
