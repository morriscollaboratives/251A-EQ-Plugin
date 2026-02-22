# Langevin EQ-251A

Analog-modeled VST3/AU plugin emulating the 1961 Langevin EQ-251A passive LC program equalizer. 64-bit double-precision, 4× oversampled, derived from schematic analysis of the original bridged-T topology. Custom vintage skeuomorphic GUI.

**Formats:** VST3 (Windows, macOS, Linux) + Audio Unit (macOS)

![License](https://img.shields.io/badge/license-MIT-blue)

---

## Download Pre-built Plugins

Go to [**Releases**](../../releases) and download the zip for your platform. No compiling needed.

| Platform | File | Install Location |
|----------|------|-----------------|
| Windows  | `LangevinEQ251A-win-x64.zip` | `C:\Program Files\Common Files\VST3\` |
| macOS    | `LangevinEQ251A-mac-universal.zip` | `~/Library/Audio/Plug-Ins/VST3/` |
| Linux    | `LangevinEQ251A-linux-x64.zip` | `~/.vst3/` |

Copy the `LangevinEQ251A.vst3` folder from the zip into the install location, restart your DAW, and scan for plugins.

On macOS, if you also get a `Langevin EQ-251A.component` in the zip, copy it to `~/Library/Audio/Plug-Ins/Components/` for Audio Unit support in Logic/GarageBand.

---

## Build From Source

### Prerequisites

All platforms need **Git** and **CMake 3.25+**.

| Platform | Additional Requirements |
|----------|----------------------|
| Windows  | [Visual Studio 2022](https://visualstudio.microsoft.com/) with **"Desktop development with C++"** |
| macOS    | Xcode (from App Store) |
| Linux    | `sudo apt install cmake g++ git libx11-dev libxcb-util0-dev libxcb-cursor-dev libxcb-keysyms1-dev libxcb-xkb-dev libxkbcommon-dev libxkbcommon-x11-dev libcairo2-dev libpango1.0-dev libfontconfig1-dev libfreetype-dev libgtkmm-3.0-dev libsqlite3-dev` |

### One-line Build

Clone this repo and run the build script for your platform:

```bash
git clone https://github.com/YOUR_USERNAME/LangevinEQ251A.git
cd LangevinEQ251A
```

**Windows** — double-click `build-windows.bat`, or from Command Prompt:
```
build-windows.bat
```

**macOS:**
```bash
chmod +x build-macos.sh
./build-macos.sh
```

**Linux:**
```bash
chmod +x build-linux.sh
./build-linux.sh
```

Each script downloads the VST3 SDK automatically, builds the plugin, and installs it to the standard location. Restart your DAW and scan for new plugins.

### Manual Build

If you prefer to run the steps yourself:

```bash
git clone --recursive --depth 1 https://github.com/steinbergmedia/vst3sdk.git
mkdir build && cd build
```

Then configure for your platform:

```bash
# Windows (from Developer Command Prompt)
cmake .. -A x64

# macOS
cmake .. -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"

# Linux
cmake .. -DCMAKE_BUILD_TYPE=Release
```

Then build:

```bash
cmake --build . --config Release --target LangevinEQ251A
```

Output lands in `build/VST3/Release/LangevinEQ251A.vst3/`.

---

## Parameters

| Control | Range | Options |
|---------|-------|---------|
| LF Gain | ±14 dB | Continuous |
| LF Freq | — | 40 Hz / 100 Hz |
| HF Gain | ±14 dB | Continuous |
| HF Freq | — | 3k / 5k / 10k / 15k Hz |
| Bypass  | — | On / Off |

---

## Technical Details

- **64-bit** double-precision signal path
- **4× oversampling** with 192-tap Kaiser FIR (β=10, >128 dB image rejection)
- **47 samples latency** at 44.1 kHz (1.07 ms), reported to host for automatic delay compensation
- **Bilinear transform** with frequency pre-warping for accurate analog response
- **Proportional-Q** from passive LC bridged-T topology — bandwidth narrows as you boost, matching the original hardware behavior
- **Click-free bypass** — signal always routes through the oversampler for consistent latency

**LF section:** First-order low shelf derived from L3 + C3 shunt leg of the original circuit.

**HF section:** Second-order peaking bell from the LC resonant shunt. Both boost and cut produce a bell shape (not shelving), consistent with the bridged-T topology and confirmed by Rane Note 122.

---

## Project Structure

```
├── CMakeLists.txt                  Top-level build (wraps VST3 SDK)
├── build-windows.bat               One-click Windows build
├── build-macos.sh                  One-click macOS build (VST3 + AU)
├── build-linux.sh                  One-click Linux build
├── .github/workflows/build.yml     CI: builds all platforms, creates releases
├── LICENSE
└── langevin-eq251a/
    ├── CMakeLists.txt              Plugin CMake targets
    ├── resource/
    │   └── au-info.plist           Audio Unit descriptor
    └── src/
        ├── dsp/LangevinDSP.h       DSP engine (header-only)
        ├── gui/
        │   ├── LangevinEditor.h     VSTGUI editor
        │   └── LangevinEditor.cpp   Custom vintage controls
        ├── processor.h/.cpp        VST3 audio processor
        ├── controller.h/.cpp       VST3 edit controller
        ├── entry.cpp               Plugin factory
        ├── plugids.h               Class & parameter IDs
        └── version.h               Version info
```

---

## License

MIT — see [LICENSE](LICENSE).

Morris Collaboratives, 2026
