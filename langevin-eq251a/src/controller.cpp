// ==========================================================================
// controller.cpp — VST3 Edit Controller for Langevin EQ-251A
// ==========================================================================

#include "controller.h"
#include "plugids.h"
#include "gui/LangevinEditor.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace Langevin {

// --------------------------------------------------------------------------
tresult PLUGIN_API LangevinController::initialize(FUnknown* context) {
    tresult result = EditController::initialize(context);
    if (result != kResultOk)
        return result;

    // --- LF GAIN ---
    // Range: -14 to +14 dB, default 0 (center = 0.5 normalized)
    // The original hardware has 15 positions (7 cut, flat, 7 boost)
    // with non-linear spacing from the resistor ladder.
    // We allow continuous control for flexibility.
    auto* lfGainParam = new RangeParameter(
        STR16("LF Gain"), kLFGain, STR16("dB"),
        -14.0, 14.0, 0.0,  // min, max, default
        0,                  // step count (0 = continuous)
        ParameterInfo::kCanAutomate
    );
    parameters.addParameter(lfGainParam);

    // --- LF FREQUENCY ---
    // Two positions: 40 Hz, 100 Hz
    auto* lfFreqParam = new StringListParameter(
        STR16("LF Freq"), kLFFreq,
        nullptr,   // units
        ParameterInfo::kCanAutomate | ParameterInfo::kIsList
    );
    lfFreqParam->appendString(STR16("40 Hz"));
    lfFreqParam->appendString(STR16("100 Hz"));
    parameters.addParameter(lfFreqParam);

    // --- HF GAIN ---
    // Range: -14 to +14 dB, default 0
    auto* hfGainParam = new RangeParameter(
        STR16("HF Gain"), kHFGain, STR16("dB"),
        -14.0, 14.0, 0.0,
        0,
        ParameterInfo::kCanAutomate
    );
    parameters.addParameter(hfGainParam);

    // --- HF FREQUENCY ---
    // Four positions: 3 kHz, 5 kHz, 10 kHz, 15 kHz
    auto* hfFreqParam = new StringListParameter(
        STR16("HF Freq"), kHFFreq,
        nullptr,
        ParameterInfo::kCanAutomate | ParameterInfo::kIsList
    );
    hfFreqParam->appendString(STR16("3 kHz"));
    hfFreqParam->appendString(STR16("5 kHz"));
    hfFreqParam->appendString(STR16("10 kHz"));
    hfFreqParam->appendString(STR16("15 kHz"));
    parameters.addParameter(hfFreqParam);

    // --- BYPASS ---
    auto* bypassParam = new RangeParameter(
        STR16("Bypass"), kBypass, nullptr,
        0.0, 1.0, 0.0,
        1,  // step count = 1 (toggle)
        ParameterInfo::kCanAutomate | ParameterInfo::kIsBypass
    );
    parameters.addParameter(bypassParam);

    return kResultOk;
}

// --------------------------------------------------------------------------
tresult PLUGIN_API LangevinController::setComponentState(IBStream* state) {
    if (!state)
        return kResultFalse;

    IBStreamer streamer(state, kLittleEndian);

    double savedLFGain = 0.0, savedHFGain = 0.0;
    int32 savedLFFreq = 0, savedHFFreq = 0;
    int32 savedBypass = 0;

    if (!streamer.readDouble(savedLFGain)) return kResultFalse;
    if (!streamer.readInt32(savedLFFreq)) return kResultFalse;
    if (!streamer.readDouble(savedHFGain)) return kResultFalse;
    if (!streamer.readInt32(savedHFFreq)) return kResultFalse;
    if (!streamer.readInt32(savedBypass)) return kResultFalse;

    // Convert to normalized values and set
    // LF Gain: [-14, 14] → [0, 1]
    setParamNormalized(kLFGain, (savedLFGain + 14.0) / 28.0);

    // LF Freq: index → normalized (0 or 1 for 2-item list)
    setParamNormalized(kLFFreq, (double)savedLFFreq);

    // HF Gain: [-14, 14] → [0, 1]
    setParamNormalized(kHFGain, (savedHFGain + 14.0) / 28.0);

    // HF Freq: index → normalized (0, 0.333, 0.666, 1 for 4-item list)
    setParamNormalized(kHFFreq, (double)savedHFFreq / 3.0);

    // Bypass: 0 or 1
    setParamNormalized(kBypass, savedBypass ? 1.0 : 0.0);

    return kResultOk;
}

// --------------------------------------------------------------------------
IPlugView* PLUGIN_API LangevinController::createView(FIDString name) {
    if (FIDStringsEqual(name, ViewType::kEditor)) {
        auto* editor = new LangevinEditor(this);
        guiEditor_ = editor;
        return editor;
    }
    return nullptr;
}

// --------------------------------------------------------------------------
tresult PLUGIN_API LangevinController::setParamNormalized(
    Steinberg::Vst::ParamID tag, Steinberg::Vst::ParamValue value)
{
    tresult result = EditController::setParamNormalized(tag, value);
    if (result == kResultOk && guiEditor_) {
        guiEditor_->syncParameterValue(tag, value);
    }
    return result;
}

} // namespace Langevin
