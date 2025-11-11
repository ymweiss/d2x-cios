# Building d2x cIOS WAD Files

This document describes how to build installable WAD files from the compiled d2x cIOS modules using WiiPy.

## Overview

After building d2x cIOS modules with `maked2x.sh`, you can package them into installable WAD files using the `build-wad.sh` script. This script:

1. Downloads the appropriate base IOS from Nintendo Update Servers (NUS)
2. Applies d2x patches to the base IOS
3. Injects custom d2x modules (MLOAD, FAT, EHCI, DIPP, ES, FFSP, USBS)
4. Creates a fakesigned WAD file ready for installation

## Prerequisites

1. **d2x cIOS built**: Run `./maked2x.sh <version> <minor>` first
2. **WiiPy cloned**: WiiPy must be in `../WiiPy` relative to d2x-cios
3. **Internet connection**: Required to download base IOS from NUS

## Usage

```bash
./build-wad.sh [base_ios] [slot] [version] [platform] [--no-ohci]
```

### Parameters

- **base_ios**: Base IOS number (default: 56)
  - **Wii**: 37, 38, 53, 55, 56, 57, 58, 60, 70, 80
  - **vWii**: 38, 56, 57, 58
- **slot**: Installation slot 3-255 (default: 249)
  - Common slots: 249, 250, 251
- **version**: cIOS version 1-65535 (default: 21999)
- **platform**: Target platform (default: wii)
  - `wii` - Original Nintendo Wii
  - `vwii` - Wii U virtual Wii mode
- **--no-ohci**: (optional) Replace stock OHCI module with EHCI
  - Removes USB 1.1 OHCI support, keeps only USB 2.0 EHCI
  - Slightly reduces WAD size
  - Recommended for modern USB devices (all support USB 2.0)

### Examples

```bash
# Build for Wii using IOS56 base, install to slot 249
./build-wad.sh 56 249 21999 wii

# Build for vWii using IOS58 base, install to slot 249
./build-wad.sh 58 249 21999 vwii

# Build for Wii using IOS57 base, install to slot 250, no OHCI
./build-wad.sh 57 250 21999 wii --no-ohci

# Build for vWii without OHCI module (EHCI only)
./build-wad.sh 58 249 21999 vwii --no-ohci

# Use defaults (IOS56, slot 249, v21999, wii platform)
./build-wad.sh
```

## Platform Differences

### Wii vs vWii Title IDs

The script automatically uses the correct Title ID format:

- **Wii IOS**: `0000000100000000` + IOS number
  - Example: IOS 58 = `000000010000003A`
- **vWii IOS**: `0000000700000000` + IOS number
  - Example: vWii IOS 58 = `000000070000003A`

### NUS Endpoints

- **Wii**: `http://nus.cdn.shop.wii.com/ccs/download/`
- **Wii U (vWii)**: `http://ccs.cdn.wup.shop.nintendo.net/ccs/download/`

### Version Differences

vWii IOSes have different versions than their Wii counterparts:

| IOS | Wii Version | vWii Version |
|-----|-------------|--------------|
| 38  | v4123/v4124 | v4380        |
| 56  | v5661       | v5918        |
| 57  | v5918/v5919 | v6175        |
| 58  | v6175/v6176 | v6432        |

## Output

WAD files are created in the WiiPy directory with this naming format:

```
d2x-cIOS{slot}[{base_ios}]-v{version}-{platform}.wad
```

Examples:
- `d2x-cIOS249[58]-v21999-vwii.wad` - vWii IOS58 base, slot 249
- `d2x-cIOS249[56]-v21999-wii.wad` - Wii IOS56 base, slot 249
- `d2x-cIOS250[57]-v21999-wii.wad` - Wii IOS57 base, slot 250

## Installation

Install the generated WAD using any of these methods:

1. **d2x cIOS Installer** (recommended)
   - Official installer with built-in verification
   - Available from WiiBrew

2. **Wii Mod Lite**
   - General-purpose WAD manager
   - User-friendly interface

3. **Any WAD Manager**
   - Multi-Mod Manager (MMM)
   - YAWMM (Yet Another Wad Manager)

### Installation Steps

1. Copy the WAD file to your SD card
2. Launch your WAD manager from Homebrew Channel
3. Navigate to the WAD file
4. Install to the specified slot
5. Verify installation succeeded

## Recommended Slot Configuration

Standard d2x cIOS setup uses these slots:

- **Slot 249**: IOS56 base (USB Loader GX, WiiFlow)
- **Slot 250**: IOS57 base (Some USB loaders)
- **Slot 251**: IOS58 base (Nintendont, some homebrew)

For vWii, typically use:
- **Slot 249**: IOS58 base

## OHCI Module Replacement

### Default Behavior (OHCI + EHCI)

By default, the script includes both USB modules:
- **OHCI** - USB 1.1 support (stock module at content ID 3)
- **EHCI** - USB 2.0 support (d2x custom module)

### --no-ohci Flag (Runtime OHCI Selection)

When `--no-ohci` is specified:
- **EHCI replaces OHCI** at content ID 3 in the WAD
- Stock OHCI module is removed from the static installation
- **mload gains runtime flexibility** to decide which OHCI to load:
  - If system call modifications succeed → Load custom HAI-IOS OHCI version
  - If system call modifications fail → Load stock OHCI module
- Enables advanced IOS customization workflows

**Technical Implementation:**
- The script modifies the ciosmaps XML to change EHCI's content ID from its normal value (e.g., `0x25`, `0x11`) to `0x3` (OHCI's slot)
- WiiPy replaces content 3 (OHCI) with EHCI instead of adding EHCI as a new content
- No content ID gaps are created (content IDs don't need to be sequential)
- mload can then dynamically load OHCI modules at runtime based on system state

**Use Case:**
- Advanced homebrew development requiring custom IOS modifications
- HAI-Riivolution or similar projects that patch IOS at runtime
- Allows fallback to stock OHCI if custom modifications fail
- Provides flexibility without requiring separate cIOS installations

### All Included Modules

The WAD includes these d2x custom modules:
- MLOAD - Module loader (enables runtime module loading)
- FAT - FAT filesystem support
- EHCI - USB 2.0 Enhanced Host Controller Interface
- DIPP - Disc interface plugin
- ES - ES signature patching
- FFSP - File system plugin
- USBS - USB storage driver

## Troubleshooting

### "Base IOS WAD not found" - Downloads from NUS
This is **normal**. The script automatically downloads the base IOS if not cached.

### "Permission denied" when writing WAD
Ensure WiiPy directory is writable by your user account.

### "The requested Title ID or TMD version does not exist"
- Check your internet connection
- Verify the base IOS and version combination is valid
- For vWii, ensure you're using vWii-supported IOS numbers (38, 56, 57, 58)

### "cIOS map file does not exist"
Run `./maked2x.sh` to build d2x cIOS modules first.

## Technical Details

The script performs these operations:

1. **Validates inputs**: Checks IOS number, slot, version, and platform
2. **Downloads base IOS**: Uses libWiiPy to fetch from NUS if not cached
3. **Applies patches**: Uses ciosmaps.xml (Wii) or ciosmaps-vWii.xml (vWii)
4. **Injects modules**: Replaces/adds d2x custom modules
5. **Sets metadata**: Configures slot number and version
6. **Fakesigns**: Signs with trucha bug signature
7. **Packs WAD**: Creates final installable WAD file

## See Also

- [d2x cIOS Build Instructions](CLAUDE.md)
- [WiiPy Documentation](https://github.com/NinjaCheetah/WiiPy)
- [WiiBrew d2x cIOS](https://wiibrew.org/wiki/D2x_cIOS)
- [vWii IOS Information](https://wiibrew.org/wiki/VIOS)
