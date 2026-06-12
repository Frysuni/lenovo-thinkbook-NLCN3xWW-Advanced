# Documentation

| # | Document | What it covers |
|---|---|---|
| 1 | [Overview](01-overview.md) | The problem, the approach, what you need. |
| 2 | [Root cause](02-root-cause.md) | The filter instruction, the flag table, the `SendForm` call path, the fix. |
| 3 | [Reproduce the analysis](03-reproduce-analysis.md) | Extract `H2OFormBrowserDxe`, run the analyzer, derive the patch for any firmware. |
| 4 | [Build & run](04-build-and-run.md) | Build the patcher, prep the USB stick, the UEFI Shell steps, troubleshooting. |
| 5 | [Cross-version matrix](05-cross-version-matrix.md) | Which `NLCN` builds hide Advanced, with module hashes and offsets. |
| 6 | [Safety & limitations](06-safety-and-limitations.md) | What it does/doesn't touch, volatility, Secure Boot, real risks. |
| 7 | [Firmware downloads](07-firmware-downloads.md) | Where to get the BIOS packages and how to extract the needed files. |
| 8 | [UEFI Shell access](08-uefi-shell-access.md) | How to reach the UEFI Shell from Windows or Linux (USB prep, boot keys, Secure Boot). |

🇷🇺 Документация на русском: [docs/ru/](ru/README.md)

New here? Start with the [Overview](01-overview.md), then
[UEFI Shell access](08-uefi-shell-access.md) and [Build &
run](04-build-and-run.md) to use it, or [Root cause](02-root-cause.md) for the
full technical story.
