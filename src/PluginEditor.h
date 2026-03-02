#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

//==============================================================================
class VintageKnob : public juce::Slider
{
public:
    explicit VintageKnob(const juce::String& label);
    void paint(juce::Graphics& g) override;

private:
    juce::String label_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VintageKnob)
};

//==============================================================================
class VintageFreqSwitch : public juce::Slider
{
public:
    VintageFreqSwitch(const juce::StringArray& labels, int numPositions);
    void paint(juce::Graphics& g) override;

private:
    juce::StringArray labels_;
    int numPos_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VintageFreqSwitch)
};

//==============================================================================
class LangevinEditor : public juce::AudioProcessorEditor
{
public:
    explicit LangevinEditor(LangevinProcessor&);
    ~LangevinEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    LangevinProcessor& processor_;

    // Mid controls
    VintageKnob lfGainKnob_        { "GAIN  dB" };
    VintageFreqSwitch lfFreqSwitch_ { juce::StringArray{"40", "100"}, 2 };
    VintageKnob hfGainKnob_        { "GAIN  dB" };
    VintageFreqSwitch hfFreqSwitch_ { juce::StringArray{"3k", "5k", "10k", "15k"}, 4 };

    // Side controls
    VintageKnob lfGainKnobS_        { "GAIN  dB" };
    VintageFreqSwitch lfFreqSwitchS_ { juce::StringArray{"40", "100"}, 2 };
    VintageKnob hfGainKnobS_        { "GAIN  dB" };
    VintageFreqSwitch hfFreqSwitchS_ { juce::StringArray{"3k", "5k", "10k", "15k"}, 4 };

    // Buttons
    juce::TextButton stereoBtn_ { "STEREO" };
    juce::TextButton bypassBtn_ { "BYPASS" };
    juce::TextButton msBtn_     { "MID/SIDE" };

    // APVTS slider attachments
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAtt> lfGainAtt_, hfGainAtt_, lfFreqAtt_, hfFreqAtt_;
    std::unique_ptr<SliderAtt> lfGainSAtt_, hfGainSAtt_, lfFreqSAtt_, hfFreqSAtt_;

    void updateModeButtons();
    void updateBypassButton();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LangevinEditor)
};
