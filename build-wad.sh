#!/bin/bash
# build-wad.sh - Build d2x cIOS WAD from built modules using WiiPy
#
# Usage: ./build-wad.sh [base_ios] [slot] [version] [platform]
#   base_ios: Base IOS number (37, 38, 53, 55, 56, 57, 58, 60, 70, 80) - default: 56
#   slot:     Installation slot (3-255) - default: 249
#   version:  cIOS version (1-65535) - default: 21999
#   platform: Target platform (wii or vwii) - default: wii
#
# Examples:
#   ./build-wad.sh 56 249 21999 wii    # Build for Wii using IOS56
#   ./build-wad.sh 58 249 21999 vwii   # Build for vWii using IOS58

set -e

# Configuration
BASE_IOS="${1:-56}"
SLOT="${2:-249}"
VERSION="${3:-21999}"
PLATFORM="${4:-wii}"
CIOS_NAME="d2x-v999-test-build"

# Paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build/$CIOS_NAME"
WIIPY_DIR="$SCRIPT_DIR/../WiiPy"
OUTPUT_WAD="$WIIPY_DIR/d2x-cIOS${SLOT}[${BASE_IOS}]-v${VERSION}-${PLATFORM}.wad"

# Check if WiiPy exists
if [ ! -d "$WIIPY_DIR" ]; then
    echo "Error: WiiPy directory not found at $WIIPY_DIR"
    echo "Please ensure WiiPy is cloned to ../WiiPy"
    exit 1
fi

# Check if d2x modules exist
if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: d2x-cios modules not found at $BUILD_DIR"
    echo "Please build d2x-cios first using: ./maked2x.sh 999 test-build"
    exit 1
fi

# Check required modules
REQUIRED_MODULES=("DIPP.app" "EHCI.app" "ES.app" "FAT.app" "FFSP.app" "MLOAD.app" "USBS.app")
for module in "${REQUIRED_MODULES[@]}"; do
    if [ ! -f "$BUILD_DIR/$module" ]; then
        echo "Error: Required module $module not found in $BUILD_DIR"
        exit 1
    fi
done

# Normalize platform
PLATFORM=$(echo "$PLATFORM" | tr '[:upper:]' '[:lower:]')
if [ "$PLATFORM" != "wii" ] && [ "$PLATFORM" != "vwii" ]; then
    echo "Error: Invalid platform '$PLATFORM'. Must be 'wii' or 'vwii'"
    exit 1
fi

# Select appropriate ciosmaps file
if [ "$PLATFORM" = "vwii" ]; then
    CIOSMAPS_FILE="$SCRIPT_DIR/build/ciosmaps-vWii.xml"
    NUS_FLAGS=""  # Use default Wii U endpoint
else
    CIOSMAPS_FILE="$SCRIPT_DIR/build/ciosmaps.xml"
    NUS_FLAGS="--wii"  # Use original Wii endpoint
fi

# Determine base IOS version from ciosmaps.xml based on platform
if [ "$PLATFORM" = "vwii" ]; then
    case "$BASE_IOS" in
        38) BASE_VERSION="4380" ;;
        56) BASE_VERSION="5918" ;;
        57) BASE_VERSION="6175" ;;
        58) BASE_VERSION="6432" ;;
        *)
            echo "Error: Unsupported vWii base IOS $BASE_IOS"
            echo "Supported vWii bases: 38, 56, 57, 58"
            exit 1
            ;;
    esac
else
    case "$BASE_IOS" in
        37) BASE_VERSION="5662" ;;
        38) BASE_VERSION="4123" ;;
        53) BASE_VERSION="5662" ;;
        55) BASE_VERSION="5662" ;;
        56) BASE_VERSION="5661" ;;
        57) BASE_VERSION="5918" ;;
        58) BASE_VERSION="6175" ;;
        60) BASE_VERSION="6174" ;;
        70) BASE_VERSION="6687" ;;
        80) BASE_VERSION="6943" ;;
        *)
            echo "Error: Unsupported Wii base IOS $BASE_IOS"
            echo "Supported Wii bases: 37, 38, 53, 55, 56, 57, 58, 60, 70, 80"
            exit 1
            ;;
    esac
fi

echo "========================================="
echo "d2x cIOS WAD Builder"
echo "========================================="
echo "Platform:    ${PLATFORM^^}"
echo "Base IOS:    IOS${BASE_IOS} v${BASE_VERSION}"
echo "Target Slot: ${SLOT}"
echo "Version:     ${VERSION}"
echo "cIOS Name:   ${CIOS_NAME}"
echo "cIOS Maps:   $(basename $CIOSMAPS_FILE)"
echo "Output:      ${OUTPUT_WAD}"
echo "========================================="

# Check if base IOS WAD exists
BASE_WAD="$WIIPY_DIR/IOS${BASE_IOS}-64-v${BASE_VERSION}.wad"
if [ ! -f "$BASE_WAD" ]; then
    echo ""
    echo "Base IOS WAD not found: $BASE_WAD"
    echo "Downloading from NUS ($([ "$PLATFORM" = "vwii" ] && echo "Wii U endpoint" || echo "Wii endpoint"))..."

    # Build title ID for IOS
    # Wii:  0000000100000000 + IOS number in hex
    # vWii: 0000000700000000 + IOS number in hex
    if [ "$PLATFORM" = "vwii" ]; then
        TID=$(printf "00000007000000%02X" "$BASE_IOS")
    else
        TID=$(printf "0000000100000%02X" "$BASE_IOS")
    fi

    cd "$WIIPY_DIR"
    python3 wiipy.py nus title "$TID" \
        --version "$BASE_VERSION" \
        --wad "IOS${BASE_IOS}-64-v${BASE_VERSION}.wad" \
        $NUS_FLAGS

    if [ ! -f "$BASE_WAD" ]; then
        echo "Error: Failed to download base IOS from NUS"
        exit 1
    fi
    echo "Base IOS downloaded successfully!"
fi

# Build the cIOS WAD using WiiPy
echo ""
echo "Building cIOS WAD..."
cd "$WIIPY_DIR"
python3 wiipy.py cios \
    "$BASE_WAD" \
    "$CIOSMAPS_FILE" \
    "$OUTPUT_WAD" \
    --cios-ver "$CIOS_NAME" \
    --modules "$BUILD_DIR" \
    --slot "$SLOT" \
    --version "$VERSION"

if [ -f "$OUTPUT_WAD" ]; then
    echo ""
    echo "========================================="
    echo "SUCCESS!"
    echo "========================================="
    echo "cIOS WAD created: $OUTPUT_WAD"
    ls -lh "$OUTPUT_WAD"
    echo ""
    echo "Install this WAD using:"
    echo "  - d2x cIOS Installer"
    echo "  - Wii Mod Lite"
    echo "  - Any WAD manager"
else
    echo "Error: WAD build failed"
    exit 1
fi
