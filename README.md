<img src="assets/app_icon_120.png" align="right" width="120" alt="MSI_FB">

# MSI_FB
**Minimal Full Fan Blast toggle for MSI laptops**

Press the "**User Scenario**" button to instantly switch between **Auto** and **Full Fan Blast**.

## Why?
I removed **MSI Center Pro** and the **MSI SDK**, so I lost the ability to control the fan.

While [**YAMDCC**](https://codeberg.org/Sparronator9999/YAMDCC) already supports this functionality, I only needed the Full Fan Blast feature. This project reimplements that single feature using **PawnIO**, keeping it lightweight and dependency-free.

## Requirements

- Windows (x64)
- [PawnIO](https://github.com/namazso/PawnIO) installed (the included `PawnIO_setup.exe` handles this)
- Run as Administrator

## Installation

1. Extract the zip anywhere (e.g. `C:\Program Files\MSI_FB`)
2. Right-click `install.bat` and **Run as administrator**
   - Or run `install.ps1` in an elevated PowerShell
3. Done. MSI_FB starts automatically on login.

## Usage

| Action            | How                               |
|-------------------|-----------------------------------|
| Toggle Full Blast | Press "**User Scenario**" button  |
| Toggle Full Blast | Double-click the tray icon        |
| Exit              | Right-click tray icon, click Exit |

### Command-line options

```
msi_fb.exe --help        Show help
msi_fb.exe --install     Register scheduled task + startup entry
msi_fb.exe --uninstall   Remove scheduled task + startup entry
msi_fb.exe --selftest    Inject a test key press on startup
msi_fb.exe --pawnio-test Test PawnIO EC access
msi_fb.exe --debug       Show debug console
```

## Uninstall

- Run `msi_fb.exe --uninstall`, or
- Use **Settings > Apps > MSI_FB > Uninstall**, or
- Use Revo Uninstaller / any uninstaller that reads the Add/Remove Programs registry

## How it works
MSI_FB uses PawnIO to read/write the Embedded Controller (EC) over port I/O (ports `0x66`/`0x62`). It toggles bit 7 of EC register `0x98` which controls the fan full-blast mode on MSI laptops.

All credit to YAMDCC for the EC logic - I just ripped the code from them.
