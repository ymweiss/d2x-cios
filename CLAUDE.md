# CLAUDE.md - d2x cIOS

This file provides guidance to Claude Code when working with the d2x cIOS codebase.

## Project Overview

d2x cIOS is a custom IOS (Input/Output System) for the Nintendo Wii console. It's an enhanced version of cIOSX rev21 by Waninkoko, designed exclusively for homebrew software. The cIOS provides features not available in official IOS versions, including USB loading support, WBFS filesystem access, and enhanced hardware capabilities.

## Architecture

d2x cIOS consists of multiple ARM-based IOS modules that run on the Wii's Starlet coprocessor (ARM926EJ-S):

- **DIPP** (dip-plugin) - Disc Interface Plugin for disc emulation and patching
- **EHCI** (ehci-module) - Enhanced Host Controller Interface for USB 2.0 support
- **ES** (es-plugin) - ES (Encryption System) plugin for signature patching
- **FAT** (fat-module) - FAT filesystem support (based on FatFs by ChaN)
- **FFSP** (ffs-plugin) - File system plugin for ISFS/NAND access
- **MLOAD** (mload-module) - Module loader for dynamic IOS module loading
- **USBS** (usb-module) - USB storage driver

Additionally:
- **cios-lib** - Shared library code used by all modules

## Build Requirements

### Critical Toolchain Version

**d2x cIOS REQUIRES devkitARM release 32 (gcc 4.5.1) from 2010.**

Using newer toolchain versions will cause compilation errors and runtime failures. The code relies on specific compiler behavior and ABI from this exact version.

### Required Tools

1. **devkitARM r32** - ARM cross-compiler toolchain
   - Source: https://github.com/Leseratte10/compile-devkitARM-r32
   - This repository contains Dockerfiles for building/using devkitARM r32

2. **stripios** - IOS ELF stripper tool
   - Located in: `stripios_src/`
   - Compile with: `g++ main.cpp -o stripios`
   - Modified version by Leseratte with fixes

3. **Standard build tools**
   - make, awk, bash

## Docker Build Setup

### Build devkitARM r32 Container

The devkitARM r32 toolchain is no longer available from official sources. Use the community-maintained Docker setup:

1. **Clone the devkitARM r32 repository:**
   ```bash
   git clone https://github.com/Leseratte10/compile-devkitARM-r32.git
   cd compile-devkitARM-r32
   ```

2. **Build the Docker image:**
   ```bash
   docker build -t devkitarm-r32 .
   ```
   This creates a container with devkitARM r32 installed at `/opt/devkitARM`.

### Build d2x cIOS Using Docker

Once you have the devkitARM r32 container:

```bash
cd /path/to/d2x-cios

# Run the build inside the container
docker run --rm -v "$PWD:/build" devkitarm-r32 bash -c "
  cd /build && \
  export PATH=/opt/devkitARM/bin:\$PATH && \
  export DEVKITARM=/opt/devkitARM && \
  ./maked2x.sh <major_version> <minor_version>"
```

Example:
```bash
docker run --rm -v "$PWD:/build" devkitarm-r32 bash -c "
  cd /build && \
  export PATH=/opt/devkitARM/bin:\$PATH && \
  export DEVKITARM=/opt/devkitARM && \
  ./maked2x.sh 11 beta1"
```

## Build Commands

### Clean Build
```bash
./maked2x.sh clean
```

### Standard Build
```bash
./maked2x.sh <major_version> <minor_version>
```

Parameters:
- `<major_version>`: Numeric major version (e.g., 11)
- `<minor_version>`: Minor version string (e.g., "beta1", "final")
- Default values: major=999, minor="unknown"

### Distribution Build
```bash
./maked2x.sh <major_version> <minor_version> dist
```

This creates a distribution package compatible with ModMii installer. Requires:
- ModMii installed
- `MODMII` environment variable set to ModMii directory
- Internet connection

## Pre-Commit Build Verification

This repository includes a pre-commit hook that automatically verifies the build succeeds before allowing commits.

**Location:** `.git/hooks/pre-commit`

The hook will:
1. Check if Docker and devkitarm-r32 image are available
2. Build d2x-cios with a test version number
3. Block the commit if build fails
4. Clean up build artifacts after verification

**To skip the check** (not recommended):
```bash
git commit --no-verify
```

**First-time setup:**
The pre-commit hook is automatically installed when you clone the repository. No additional setup needed.

## Build Output

Successful build creates in `build/d2x-v<major>-<minor>/`:

- `DIPP.app` - DIP plugin module (7-8 KB)
- `EHCI.app` - EHCI USB module (16-17 KB)
- `ES.app` - ES plugin module (4-5 KB)
- `FAT.app` - FAT filesystem module (19-20 KB)
- `FFSP.app` - FFS plugin module (8-9 KB)
- `MLOAD.app` - Module loader (8-9 KB)
- `USBS.app` - USB storage module (11-12 KB)
- `d2x-beta.bat` - Installation batch script
- `ciosmaps.xml` - Module mapping configuration
- `ciosmaps-vWii.xml` - vWii module mapping
- `ReadMe.txt` - Installation instructions
- `Changelog.txt` - Version history

## Code Structure

```
d2x-cios/
├── source/
│   ├── cios-lib/          # Shared library code
│   ├── dip-plugin/        # Disc interface plugin
│   ├── ehci-module/       # USB EHCI driver
│   ├── es-plugin/         # ES signature patching
│   ├── fat-module/        # FAT filesystem (FatFs)
│   ├── ffs-plugin/        # File system plugin
│   ├── mload-module/      # Module loader
│   └── usb-module/        # USB storage driver
├── data/                  # Template files for build
├── stripios_src/          # IOS ELF stripper source
├── maked2x.sh            # Main build script
└── replace.awk           # Variable replacement script
```

## Important Notes

### DO NOT modify:
- Toolchain version (must use devkitARM r32)
- Module base addresses in code
- IOS syscall numbers and signatures
- Memory alignment requirements

### Module Loading
Modules are loaded by the mload-module at runtime into specific memory regions in IOS. Each module has hardcoded memory addresses that must not be changed without understanding the full IOS memory map.

### Testing
d2x cIOS modules run in IOS context on actual Wii hardware. There is no emulator that accurately replicates IOS behavior. Testing requires:
- Real Wii or Wii U (vWii mode)
- d2x cIOS installer
- Caution: Improper IOS modifications can brick the console

## Commit Message Format

Follow standard git conventions:
- Use clear, descriptive commit messages
- Reference issue numbers when applicable
- Describe what changed and why

## Security Context

This codebase contains:
- IOS system modifications
- Signature verification bypasses
- Copy protection circumvention

This is legitimate homebrew software for:
- Running homebrew applications
- Game preservation and backups
- Research and educational purposes

**Never** use this for piracy or unauthorized distribution of copyrighted content.

## References

- Original d2x repository: https://github.com/davebaol/d2x-cios
- Wiidev maintained fork: https://github.com/wiidev/d2x-cios
- devkitARM r32 tools: https://github.com/Leseratte10/compile-devkitARM-r32
- WiiBrew documentation: https://wiibrew.org/
- Custom IOS Module Toolkit: http://wiibrew.org/wiki/Custom_IOS_Module_Toolkit
- libogc documentation: https://libogc.devkitpro.org/

## Troubleshooting

### Build fails with "arm-eabi-gcc: command not found"
- Ensure `DEVKITARM` is set: `export DEVKITARM=/opt/devkitARM`
- Ensure ARM compiler is in PATH: `export PATH=$DEVKITARM/bin:$PATH`

### Build fails with newer devkitARM
- d2x cIOS **requires** devkitARM r32 (gcc 4.5.1)
- Newer versions are incompatible due to compiler and ABI changes
- Use the Dockerfile from https://github.com/Leseratte10/compile-devkitARM-r32

### stripios not found
- Compile it: `cd stripios_src && g++ main.cpp -o stripios`
- Copy to project root: `cp stripios ..`
- Or install globally: `cp stripios /usr/local/bin/`

### Modules fail to load on Wii
- Verify you built with devkitARM r32 (not newer version)
- Check module sizes match expected ranges (4-20 KB per module)
- Ensure IOS base version matches (usually IOS 57 or 58)
