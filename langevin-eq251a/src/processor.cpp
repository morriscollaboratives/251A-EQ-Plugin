// ==========================================================================
// processor.cpp — VST3 Audio Processor for Langevin EQ-251A
// ==========================================================================

#include "processor.h"
#include "plugids.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include <cstring>
#include <algorithm>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace Langevin {

// --------------------------------------------------------------------------
LangevinProcessor::LangevinProcessor() {
    setControllerClass(ControllerUID);
}

// --------------------------------------------------------------------------
tresult PLUGIN_API LangevinProcessor::initialize(FUnknown* context) {
    tresult result = AudioEffect::initialize(context);
    if (result != kResultOk)
        return result;

    // Stereo in/out
    addAudioInput(STR16("Stereo In"), SpeakerArr::kStereo);
    addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);

    return kResultOk;
}

// --------------------------------------------------------------------------
tresult PLUGIN_API LangevinProcessor::terminate() {
    return AudioEffect::terminate();
}

// --------------------------------------------------------------------------
tresult PLUGIN_API LangevinProcessor::setActive(TBool state) {
    if (state) {
        dsp_.prepare(processSetup.sampleRate, processSetup.maxSamplesPerBlock);
        // Push stored parameter values and snap smoothers immediately
        // so there's no audible gain ramp on activation
        if (bypass_) {
            dsp_.setLFParams(0.0, lfFreq_);
            dsp_.setHFParams(0.0, hfFreq_);
        } else {
            dsp_.setLFParams(lfGain_, lfFreq_);
            dsp_.setHFParams(hfGain_, hfFreq_);
        }
        dsp_.snapToCurrentParams();
    } else {
        dsp_.reset();
    }
    return AudioEffect::setActive(state);
}

// --------------------------------------------------------------------------
uint32 PLUGIN_API LangevinProcessor::getLatencySamples() {
    return dsp_.latencySamples();
}

// --------------------------------------------------------------------------
tresult PLUGIN_API LangevinProcessor::setupProcessing(ProcessSetup& newSetup) {
    return AudioEffect::setupProcessing(newSetup);
}

// --------------------------------------------------------------------------
tresult PLUGIN_API LangevinProcessor::canProcessSampleSize(int32 symbolicSampleSize) {
    // Support both 32-bit and 64-bit processing
    if (symbolicSampleSize == kSample32 || symbolicSampleSize == kSample64)
        return kResultTrue;
    return kResultFalse;
}

// --------------------------------------------------------------------------
tresult PLUGIN_API LangevinProcessor::setBusArrangements(
    SpeakerArrangement* inputs, int32 numIns,
    SpeakerArrangement* outputs, int32 numOuts)
{
    // Accept stereo or mono; if mono input, we'll process as mono
    if (numIns == 1 && numOuts == 1) {
        if (inputs[0] == SpeakerArr::kStereo && outputs[0] == SpeakerArr::kStereo)
            return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
        if (inputs[0] == SpeakerArr::kMono && outputs[0] == SpeakerArr::kMono)
            return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
    }
    return kResultFalse;
}

// --------------------------------------------------------------------------
tresult PLUGIN_API LangevinProcessor::process(ProcessData& data) {
    // --- Handle parameter changes ---
    if (data.inputParameterChanges) {
        int32 numParams = data.inputParameterChanges->getParameterCount();
        for (int32 i = 0; i < numParams; ++i) {
            auto* queue = data.inputParameterChanges->getParameterData(i);
            if (!queue) continue;

            ParamValue value;
            int32 sampleOffset;
            int32 numPoints = queue->getPointCount();
            // Use last point for this block
            if (queue->getPoint(numPoints - 1, sampleOffset, value) != kResultTrue)
                continue;

            switch (queue->getParameterId()) {
                case kLFGain: {
                    // Normalized [0,1] → [-14, +14] dB
                    double v = std::max(0.0, std::min(1.0, (double)value));
                    lfGain_ = (v * 2.0 - 1.0) * 14.0;
                    break;
                }
                case kLFFreq:
                    // Normalized [0,1] → 0 or 1 (2 steps)
                    lfFreq_ = (value >= 0.5) ? 1 : 0;
                    break;
                case kHFGain: {
                    // Normalized [0,1] → [-14, +14] dB
                    double v = std::max(0.0, std::min(1.0, (double)value));
                    hfGain_ = (v * 2.0 - 1.0) * 14.0;
                    break;
                }
                case kHFFreq:
                    // Normalized [0,1] → 0,1,2,3 (4 steps)
                    hfFreq_ = std::min(3, std::max(0, (int)(value * 4.0)));
                    break;
                case kBypass:
                    bypass_ = (value >= 0.5);
                    break;
            }
        }
    }

    // --- Audio processing ---
    if (data.numInputs == 0 || data.numOutputs == 0 || data.numSamples <= 0) {
        // Silence output buffers (VST3 spec: outputs must be cleared if not processing)
        if (data.numOutputs > 0 && data.numSamples > 0) {
            int32 nc = data.outputs[0].numChannels;
            for (int32 c = 0; c < nc; ++c) {
                if (data.symbolicSampleSize == kSample64)
                    std::memset(data.outputs[0].channelBuffers64[c], 0, sizeof(double) * data.numSamples);
                else
                    std::memset(data.outputs[0].channelBuffers32[c], 0, sizeof(float) * data.numSamples);
            }
            data.outputs[0].silenceFlags = (1ULL << nc) - 1;
        }
        return kResultOk;
    }

    int32 numChannels = data.inputs[0].numChannels;
    int32 numSamples = data.numSamples;

    // Update DSP parameters — when bypassed, set gains to 0 so signal
    // passes through the oversampler at unity (maintains latency alignment)
    if (bypass_) {
        dsp_.setLFParams(0.0, lfFreq_);
        dsp_.setHFParams(0.0, hfFreq_);
    } else {
        dsp_.setLFParams(lfGain_, lfFreq_);
        dsp_.setHFParams(hfGain_, hfFreq_);
    }

    // Process audio (always through oversampler for consistent latency)
    if (data.symbolicSampleSize == kSample64) {
        processAudio(
            (double**)data.inputs[0].channelBuffers64,
            (double**)data.outputs[0].channelBuffers64,
            numChannels, numSamples);
    } else {
        processAudio(
            (float**)data.inputs[0].channelBuffers32,
            (float**)data.outputs[0].channelBuffers32,
            numChannels, numSamples);
    }

    // Clear silence flags — we always produce output when active
    data.outputs[0].silenceFlags = 0;

    return kResultOk;
}

// --------------------------------------------------------------------------
template <typename SampleType>
void LangevinProcessor::processAudio(
    SampleType** in, SampleType** out,
    int32 numChannels, int32 numSamples)
{
    dsp_.process(
        (const SampleType* const*)in,
        (SampleType* const*)out,
        numChannels, numSamples);
}

// Explicit template instantiation
template void LangevinProcessor::processAudio<float>(float**, float**, int32, int32);
template void LangevinProcessor::processAudio<double>(double**, double**, int32, int32);

// --------------------------------------------------------------------------
tresult PLUGIN_API LangevinProcessor::setState(IBStream* state) {
    if (!state) return kResultFalse;

    IBStreamer streamer(state, kLittleEndian);

    double savedLFGain = 0.0, savedHFGain = 0.0;
    int32 savedLFFreq = 0, savedHFFreq = 0;
    int32 savedBypass = 0;

    if (!streamer.readDouble(savedLFGain)) return kResultFalse;
    if (!streamer.readInt32(savedLFFreq)) return kResultFalse;
    if (!streamer.readDouble(savedHFGain)) return kResultFalse;
    if (!streamer.readInt32(savedHFFreq)) return kResultFalse;
    if (!streamer.readInt32(savedBypass)) return kResultFalse;

    lfGain_ = std::max(-14.0, std::min(14.0, savedLFGain));
    lfFreq_ = std::max((int32)0, std::min((int32)1, savedLFFreq));
    hfGain_ = std::max(-14.0, std::min(14.0, savedHFGain));
    hfFreq_ = std::max((int32)0, std::min((int32)3, savedHFFreq));
    bypass_ = (savedBypass != 0);

    return kResultOk;
}

// --------------------------------------------------------------------------
tresult PLUGIN_API LangevinProcessor::getState(IBStream* state) {
    if (!state) return kResultFalse;

    IBStreamer streamer(state, kLittleEndian);

    streamer.writeDouble(lfGain_);
    streamer.writeInt32(lfFreq_);
    streamer.writeDouble(hfGain_);
    streamer.writeInt32(hfFreq_);
    streamer.writeInt32(bypass_ ? 1 : 0);

    return kResultOk;
}

} // namespace Langevin
