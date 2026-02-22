#!/bin/bash
# ============================================================
#  Langevin EQ-251A — Linux Build Script
#
#  Prerequisites:
#    - CMake 3.25+, g++, git
#    - Ubuntu/Debian: sudo apt install cmake g++ git
#    - Fedora: sudo dnf install cmake gcc-c++ git
#
#  Run: chmod +x build-linux.sh && ./build-linux.sh
# ============================================================

set -e

echo ""
echo "============================================================"
echo "  Langevin EQ-251A — Linux Build"
echo "  Morris Collaboratives"
echo "============================================================"
echo ""

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# ---- Check prerequisites ----

for cmd in cmake g++ git; do
    if ! command -v $cmd &> /dev/null; then
        echo "ERROR: $cmd not found."
        echo "Install with: sudo apt install cmake g++ git"
        exit 1
    fi
done

# ---- Install VSTGUI dependencies (Linux) ----

echo "    Checking GUI dependencies..."
DEPS="libx11-dev libxcb1-dev libxcb-util0-dev libxcb-cursor-dev"
DEPS="$DEPS libxcb-keysyms1-dev libxcb-xkb-dev libxkbcommon-dev"
DEPS="$DEPS libxkbcommon-x11-dev libcairo2-dev libpango1.0-dev"
DEPS="$DEPS libfontconfig1-dev libfreetype-dev libgtkmm-3.0-dev libsqlite3-dev"

MISSING=""
for pkg in $DEPS; do
    if ! dpkg -s "$pkg" &>/dev/null; then
        MISSING="$MISSING $pkg"
    fi
done

if [ -n "$MISSING" ]; then
    echo "    Installing:$MISSING"
    sudo apt-get install -y -qq $MISSING
fi

# ---- Clone VST3 SDK ----

if [ ! -d "vst3sdk" ]; then
    echo "[1/4] Downloading VST3 SDK..."
    git clone --recursive --depth 1 https://github.com/steinbergmedia/vst3sdk.git
else
    echo "[1/4] VST3 SDK already present."
fi

# ---- Configure ----

echo "[2/4] Configuring..."
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release

# ---- Build ----

echo "[3/4] Building..."
cmake --build . --config Release --target LangevinEQ251A -j$(nproc)

# ---- Install ----

echo "[4/4] Installing..."

VST3_DIR="$HOME/.vst3"
BUNDLE="VST3/Release/LangevinEQ251A.vst3"

if [ -d "$BUNDLE" ]; then
    mkdir -p "$VST3_DIR"
    rm -rf "$VST3_DIR/LangevinEQ251A.vst3"
    cp -R "$BUNDLE" "$VST3_DIR/"
    echo "  Installed to: $VST3_DIR/LangevinEQ251A.vst3"
else
    echo "  Build output not found at $BUNDLE"
    exit 1
fi

cd "$SCRIPT_DIR"

echo ""
echo "============================================================"
echo "  BUILD SUCCESSFUL!"
echo ""
echo "  Restart your DAW and scan for plugins."
echo "  Appears as \"Langevin EQ-251A\" under Fx > EQ."
echo "============================================================"
