# Langevin EQ-251A — VST3 + Audio Unit Plugin

Analog-modeled emulation of the 1961 Langevin EQ-251A passive LC program equalizer.
Built with JUCE 8 for cross-platform AU and VST3 support.

Morris Collaboratives, 2026

---

## Quick Build

### macOS (VST3 + Audio Unit)

**Prerequisites:**
- Xcode (full install from App Store)
- CMake 3.25+: `brew install cmake`

```bash
mkdir build && cd build
cmake .. -GXcode \
  -DCMAKE_OSX_ARCHITECTURES="arm64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO \
  -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY=""
cmake --build . --config Release
```

Install:
```bash
cp -r LangevinEQ251A_artefacts/Release/AU/Langevin\ EQ-251A.component \
  ~/Library/Audio/Plug-Ins/Components/
killall -9 AudioComponentRegistrar 2>/dev/null
```

AU validates as: `auval -v aufx Lneq MoCo`

### Windows (VST3)

**Prerequisites:**
- CMake 3.25+
- Visual Studio 2022 with "Desktop development with C++"

```powershell
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

Install (run as admin):
```powershell
Copy-Item -Recurse -Force "LangevinEQ251A_artefacts\Release\VST3\Langevin EQ-251A.vst3" "$env:CommonProgramFiles\VST3\"
```

### Linux (VST3)

```bash
sudo apt install cmake g++ libfreetype-dev libx11-dev libxrandr-dev libxcursor-dev \
  libxinerama-dev libasound2-dev
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
cp -r LangevinEQ251A_artefacts/Release/VST3/Langevin\ EQ-251A.vst3 ~/.vst3/
```

---

## Parameters

| Parameter | Range | Type |
|-----------|-------|------|
| LF Gain | -14 to +14 dB | Continuous |
| LF Freq | 40 Hz / 100 Hz | Switch |
| HF Gain | -14 to +14 dB | Continuous |
| HF Freq | 3k / 5k / 10k / 15k Hz | Switch |
| Mode | Stereo / Mid-Side | Toggle |
| Bypass | On / Off | Toggle |

In **Stereo** mode, both channels receive identical EQ settings.
In **Mid/Side** mode, the left panel controls Mid EQ and the right panel controls Side EQ independently.

---

## DSP Architecture

- 64-bit double-precision signal path
- 8× oversampling (384-tap least-squares optimal FIR, ~-80 dB stopband)
- Bilinear transform with frequency pre-warping
- Proportional-Q from passive LC bridged-T topology
- Per-sample exponential parameter smoothing (5 ms time constant)
- Transposed Direct Form II for numerical stability
- 47 samples latency

**LF section:** First-order low shelf modeled from L3 + C3 shunt network (40/100 Hz selectable)

**HF section:** Second-order peaking bell modeled from LC resonant shunt (3k/5k/10k/15k Hz selectable). Peaks in both boost and cut, confirmed by topology analysis and Rane Note 122.

**Anti-alias FIR:** Least-squares optimal design computed via Cholesky decomposition of the Gram matrix with closed-form band integrals. Produces the smoothest possible passband (monotonic rolloff, <0.01 dB ripple) compared to Kaiser windowed-sinc or Parks-McClellan equiripple designs.

---

## Plugin IDs

| Field | Value |
|-------|-------|
| Manufacturer Code | MoCo |
| Plugin Code | Lneq |
| AU Type | aufx (Audio Effect) |
| Bundle ID | com.morriscollaboratives.langevin-eq251a |
| VST3 Categories | Fx, EQ |

---

## Project Structure

```
langevin-juce-clean/
├── CMakeLists.txt              # JUCE 8 FetchContent, AU + VST3 targets
├── build.sh                    # macOS build script
├── .gitignore
├── README.md
└── src/
    ├── PluginProcessor.h/cpp   # JUCE audio processor, APVTS parameters
    ├── PluginEditor.h/cpp      # Vintage skeuomorphic GUI (custom Graphics)
    └── dsp/
        └── LangevinDSP.h       # Core DSP engine (header-only, zero dependencies)
```

The DSP engine (`LangevinDSP.h`) has no JUCE or framework dependencies. It operates on raw double arrays and can be used standalone in any C++ project.

---

## Build Framework

This plugin uses [JUCE 8](https://juce.com/) fetched automatically via CMake FetchContent. JUCE handles:
- AU and VST3 format wrapping
- GUI rendering (cross-platform, identical on macOS/Windows/Linux)
- Parameter management and state serialization
- Host bus layout negotiation (mono + stereo)

JUCE does not process or modify audio samples. All DSP runs in `LangevinDSP.h`.

The free JUCE license is GPLv3. A paid indie license (~$40/month) allows proprietary distribution without the splash screen.

---

## License

Copyright 2026 Morris Collaboratives. MIT License.
