// ==========================================================================
// plugids.h — Class UIDs and Parameter IDs for Langevin EQ-251A
// ==========================================================================

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace Langevin {

// Class IDs (generated UUIDs)
static const Steinberg::FUID ProcessorUID  (0x4C4E4745, 0x51323531, 0x41505243, 0x00000001);
static const Steinberg::FUID ControllerUID (0x4C4E4745, 0x51323531, 0x41435452, 0x00000001);

// Parameter IDs
enum ParamID : Steinberg::Vst::ParamID {
    kLFGain   = 100,    // LF gain: -14 to +14 dB
    kLFFreq   = 101,    // LF frequency: 0=40Hz, 1=100Hz
    kHFGain   = 200,    // HF gain: -14 to +14 dB
    kHFFreq   = 201,    // HF frequency: 0=3k, 1=5k, 2=10k, 3=15k
    kBypass   = 900,    // Bypass
};

// VST3 subcategory
#define LANGEVIN_VST3_CATEGORY "Fx|EQ"

} // namespace Langevin
