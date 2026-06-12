# 7. Firmware downloads

> No firmware is included in this repository (Lenovo/Insyde copyright). Download
> the BIOS package for your own machine from Lenovo. The links below are
> starting points; Lenovo rotates the actual file URLs and gates them behind
> short-lived tokens, so always navigate from the official support page rather
> than reusing a hard-coded CDN link.

## Model identification

| Marketing name | Type | BIOS family |
|---|---|---|
| ThinkBook 16 G6+ AHP | 21LG | `NLCN` |
| ThinkBook 14 G6+ AHP | 21LF | `NLCN` |

Confirm your exact type number (sticker on the underside, or `wmic baseboard get
product` in Windows) before downloading.

## Official support pages

The G6+ AHP is primarily a China-market model; its BIOS is served from Lenovo
China. Use the support site search for your **type number** (e.g. `21LG`) or the
model name:

- Lenovo China support (drivers/BIOS):
  `https://newsupport.lenovo.com.cn/` — search `ThinkBook 16 G6+ AHP` or `21LG`,
  then **驱动下载 / BIOS**.
- Lenovo global PC support (same URL scheme as other ThinkBooks):
  `https://pcsupport.lenovo.com/` — search the type number, then
  **Drivers & Software → BIOS/UEFI**.

The package you want is the **"BIOS Update (Flash from Operating System)"**
Insyde `H2OFFT` installer named `BIOS-NLCNxxWW.exe` / `NLCNxxWW.exe`.

## Direct CDN pattern (reference only — tokens expire)

Lenovo China serves the files from:

```
https://newdriverdl.lenovo.com.cn/newlenovo/alldriversupload/<package-id>/<file>?token=<...>
```

Observed package IDs (the `<package-id>` segment), for reference when matching a
version on the support listing:

| Version | file | package id |
|---|---|---|
| NLCN22WW | `BIOS-NLCN22WW.exe` | 123091 |
| NLCN27WW | `BIOS-NLCN27WW.exe` | 128072 |

The `token` query parameter is time-limited; a bare URL without a fresh token
returns an error. Get a working link by clicking the download on the support
page.

## Extracting the flashable image and the EFI apps

The `.exe` is an Insyde `H2OFFT` self-extractor. Inside it you will find the SPI
image (`*.bin` / `*.FD` / `*.cap`) and the flash tooling. From the SPI image:

- Extract **`H2OFormBrowserDxe`** (GUID `9E5DAEB4-4B91-4466-9EBE-81C7E4401E6D`)
  for analysis — see [reproduce the analysis](03-reproduce-analysis.md).
- Extract **`SetupUtilityApp`** (the Setup DXE application's PE32 section) to run
  alongside the patcher — see [build & run](04-build-and-run.md).

Extraction is done with UEFITool / `uefiextract`, provided by the Nix dev shell
(`nix develop`).

## Note on regions and mirrors

Third-party driver mirrors exist but are unverified — prefer the official Lenovo
support page and verify the BIOS version string. If you keep local copies for
analysis, they stay out of this repository via [`.gitignore`](../.gitignore).
