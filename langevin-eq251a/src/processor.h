// ==========================================================================
// processor.h — VST3 Audio Processor for Langevin EQ-251A
// ==========================================================================

#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "dsp/LangevinDSP.h"

namespace Langevin {

class LangevinProcessor : public Steinberg::Vst::AudioEffect {
public:
    LangevinProcessor();

    static Steinberg::FUnknown* createInstance(void*) {
        return (Steinberg::Vst::IAudioProcessor*)new LangevinProcessor;
    }

    // AudioEffect overrides
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API terminate() SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup& newSetup) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setBusArrangements(
        Steinberg::Vst::SpeakerArrangement* inputs, Steinberg::int32 numIns,
        Steinberg::Vst::SpeakerArrangement* outputs, Steinberg::int32 numOuts) SMTG_OVERRIDE;
    Steinberg::uint32 PLUGIN_API getLatencySamples() SMTG_OVERRIDE;

private:
    template <typename SampleType>
    void processAudio(SampleType** in, SampleType** out,
                      Steinberg::int32 numChannels, Steinberg::int32 numSamples);

    StereoProcessor dsp_;

    // Current parameter values (set from process() parameter changes)
    double lfGain_ = 0.0;    // dB
    int    lfFreq_ = 0;      // index: 0=40Hz, 1=100Hz
    double hfGain_ = 0.0;    // dB
    int    hfFreq_ = 0;      // index: 0=3k, 1=5k, 2=10k, 3=15k
    bool   bypass_ = false;
};

} // namespace Langevin
