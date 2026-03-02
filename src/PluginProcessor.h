#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/LangevinDSP.h"

class LangevinProcessor : public juce::AudioProcessor
{
public:
    LangevinProcessor();
    ~LangevinProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    Langevin::StereoProcessor dsp_;

    std::atomic<float>* lfGainParam_      = nullptr;
    std::atomic<float>* lfFreqParam_      = nullptr;
    std::atomic<float>* hfGainParam_      = nullptr;
    std::atomic<float>* hfFreqParam_      = nullptr;
    std::atomic<float>* lfGainSParam_     = nullptr;
    std::atomic<float>* lfFreqSParam_     = nullptr;
    std::atomic<float>* hfGainSParam_     = nullptr;
    std::atomic<float>* hfFreqSParam_     = nullptr;
    std::atomic<float>* processModeParam_ = nullptr;
    std::atomic<float>* bypassParam_      = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LangevinProcessor)
};
