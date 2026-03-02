#!/bin/bash
set -e
echo "=== Langevin EQ-251A (JUCE) ==="

if ! xcode-select -p &>/dev/null; then
    echo "ERROR: Xcode not installed." && exit 1
fi
if ! command -v cmake &>/dev/null; then
    echo "ERROR: CMake not found. Get it from https://cmake.org/download/" && exit 1
fi

rm -rf build && mkdir build && cd build

echo "--- Configuring ---"
cmake .. -GXcode \
    -DCMAKE_OSX_ARCHITECTURES="arm64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0

echo "--- Building ---"
cmake --build . --config Release

echo ""
echo "=== Done ==="
echo "Install AU:   cp -r LangevinEQ251A_artefacts/Release/AU/*.component ~/Library/Audio/Plug-Ins/Components/"
echo "Install VST3: cp -r LangevinEQ251A_artefacts/Release/VST3/*.vst3 ~/Library/Audio/Plug-Ins/VST3/"
echo "Then:         killall -9 AudioComponentRegistrar 2>/dev/null; auval -v aufx Lneq MoCo"
