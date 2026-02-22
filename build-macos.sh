#!/bin/bash
# ============================================================
#  Langevin EQ-251A — macOS Build Script
#  Builds both VST3 and Audio Unit (AUv2) plugins
#
#  Prerequisites:
#    - Xcode (full install from App Store, not just CLT)
#    - CMake 3.25+ (brew install cmake)
#
#  Run: chmod +x build-macos.sh && ./build-macos.sh
# ============================================================

set -e

echo ""
echo "============================================================"
echo "  Langevin EQ-251A — macOS Build"
echo "  VST3 + Audio Unit (AUv2)"
echo "  Morris Collaboratives"
echo "============================================================"
echo ""

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# ---- Check prerequisites ----

if ! command -v cmake &> /dev/null; then
    echo "ERROR: CMake not found."
    echo "Install with: brew install cmake"
    exit 1
fi

# Check for full Xcode (required for AU builds, CLT alone won't work)
if ! xcode-select -p &> /dev/null; then
    echo "ERROR: Xcode not found."
    echo "Install Xcode from the App Store, then run:"
    echo "  sudo xcode-select -s /Applications/Xcode.app"
    exit 1
fi

XCODE_PATH=$(xcode-select -p)
if [[ "$XCODE_PATH" != *"Xcode.app"* ]]; then
    echo "WARNING: Xcode Command Line Tools detected, but full Xcode"
    echo "is recommended for Audio Unit builds."
    echo ""
    echo "Current path: $XCODE_PATH"
    echo ""
    echo "If you have Xcode installed, run:"
    echo "  sudo xcode-select -s /Applications/Xcode.app"
    echo ""
    echo "Continuing with VST3 only (AU build may fail)..."
    echo ""
fi

# ---- Clone VST3 SDK ----

if [ ! -d "vst3sdk" ]; then
    echo "[1/5] Cloning VST3 SDK..."
    git clone --recursive --depth 1 https://github.com/steinbergmedia/vst3sdk.git
else
    echo "[1/5] VST3 SDK already present."
fi

# ---- Clone Apple AudioUnit SDK (required for AU wrapper) ----

if [ ! -d "AudioUnitSDK" ]; then
    echo "[2/5] Cloning Apple AudioUnit SDK..."
    git clone --depth 1 https://github.com/apple/AudioUnitSDK.git
else
    echo "[2/5] AudioUnit SDK already present."
fi

# ---- Configure (Xcode generator for AU support) ----

echo "[3/5] Configuring CMake (Xcode, Universal Binary)..."
mkdir -p build
cd build
cmake .. -G Xcode \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="10.13" \
    -DSMTG_AUDIOUNIT_SDK_PATH="$SCRIPT_DIR/AudioUnitSDK"

# ---- Build VST3 ----

echo "[4/5] Building VST3 plugin..."
cmake --build . --config Release --target LangevinEQ251A

# ---- Build Audio Unit ----

echo "[5/5] Building Audio Unit plugin..."
AU_BUILD_OK=true
cmake --build . --config Release --target LangevinEQ251A-au || {
    echo ""
    echo "WARNING: Audio Unit build failed."
    echo "This usually means full Xcode is not configured."
    echo "The VST3 plugin was built successfully."
    AU_BUILD_OK=false
}

# ---- Install ----

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"

mkdir -p "$VST3_DIR"
mkdir -p "$AU_DIR"

echo ""
echo "============================================================"

# Install VST3
VST3_BUNDLE="VST3/Release/LangevinEQ251A.vst3"
if [ -d "$VST3_BUNDLE" ]; then
    rm -rf "$VST3_DIR/LangevinEQ251A.vst3"
    cp -R "$VST3_BUNDLE" "$VST3_DIR/"
    echo "  VST3 installed:"
    echo "    $VST3_DIR/LangevinEQ251A.vst3"
else
    echo "  VST3: build output not found"
fi

# Install AU (the SDK post-build step may have already done this,
# but we copy explicitly to be safe)
AU_BUNDLE="VST3/Release/Langevin EQ-251A.component"
if [ -d "$AU_BUNDLE" ] && [ "$AU_BUILD_OK" = true ]; then
    rm -rf "$AU_DIR/Langevin EQ-251A.component"
    cp -R "$AU_BUNDLE" "$AU_DIR/"
    echo "  Audio Unit installed:"
    echo "    $AU_DIR/Langevin EQ-251A.component"
elif [ "$AU_BUILD_OK" = true ]; then
    # Check the Components folder (SDK may copy directly there)
    if [ -d "$AU_DIR/Langevin EQ-251A.component" ]; then
        echo "  Audio Unit installed (by SDK post-build):"
        echo "    $AU_DIR/Langevin EQ-251A.component"
    fi
fi

echo ""
echo "  BUILD COMPLETE — Restart your DAW and scan for plugins."
echo ""
echo "  Plugin appears as:"
echo "    VST3:  Langevin EQ-251A  (Fx > EQ)"
echo "    AU:    Morris Collaboratives: Langevin EQ-251A  (aufx/Lneq/MoCo)"
echo "============================================================"

cd "$SCRIPT_DIR"
