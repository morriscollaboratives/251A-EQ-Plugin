#include "PluginProcessor.h"
#include "PluginEditor.h"

LangevinProcessor::LangevinProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    lfGainParam_      = apvts.getRawParameterValue("lfGain");
    lfFreqParam_      = apvts.getRawParameterValue("lfFreq");
    hfGainParam_      = apvts.getRawParameterValue("hfGain");
    hfFreqParam_      = apvts.getRawParameterValue("hfFreq");
    lfGainSParam_     = apvts.getRawParameterValue("lfGainS");
    lfFreqSParam_     = apvts.getRawParameterValue("lfFreqS");
    hfGainSParam_     = apvts.getRawParameterValue("hfGainS");
    hfFreqSParam_     = apvts.getRawParameterValue("hfFreqS");
    processModeParam_ = apvts.getRawParameterValue("processMode");
    bypassParam_      = apvts.getRawParameterValue("bypass");
}

juce::AudioProcessorValueTreeState::ParameterLayout LangevinProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Mid/Main channel
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("lfGain", 1), "LF Gain",
        juce::NormalisableRange<float>(-14.0f, 14.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("lfFreq", 1), "LF Freq",
        juce::StringArray{"40 Hz", "100 Hz"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("hfGain", 1), "HF Gain",
        juce::NormalisableRange<float>(-14.0f, 14.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("hfFreq", 1), "HF Freq",
        juce::StringArray{"3 kHz", "5 kHz", "10 kHz", "15 kHz"}, 0));

    // Side channel
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("lfGainS", 1), "Side LF Gain",
        juce::NormalisableRange<float>(-14.0f, 14.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("lfFreqS", 1), "Side LF Freq",
        juce::StringArray{"40 Hz", "100 Hz"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("hfGainS", 1), "Side HF Gain",
        juce::NormalisableRange<float>(-14.0f, 14.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("hfFreqS", 1), "Side HF Freq",
        juce::StringArray{"3 kHz", "5 kHz", "10 kHz", "15 kHz"}, 0));

    // Mode and bypass
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("processMode", 1), "Process Mode",
        juce::StringArray{"Stereo", "Mid/Side"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("bypass", 1), "Bypass", false));

    return { params.begin(), params.end() };
}

bool LangevinProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainIn  = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    // Input and output must match
    if (mainIn != mainOut)
        return false;

    // Support mono and stereo
    if (mainIn == juce::AudioChannelSet::mono())   return true;
    if (mainIn == juce::AudioChannelSet::stereo())  return true;

    return false;
}

void LangevinProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    dsp_.prepare(sampleRate, samplesPerBlock);
    dsp_.reset();

    // Apply current parameter state immediately so there's no ramp from zero
    dsp_.setProcessMode(Langevin::ProcessMode::Stereo);
    dsp_.setChannelLFParams(0, static_cast<double>(lfGainParam_->load()),
                               static_cast<int>(lfFreqParam_->load()));
    dsp_.setChannelHFParams(0, static_cast<double>(hfGainParam_->load()),
                               static_cast<int>(hfFreqParam_->load()));
    dsp_.setChannelLFParams(1, static_cast<double>(lfGainParam_->load()),
                               static_cast<int>(lfFreqParam_->load()));
    dsp_.setChannelHFParams(1, static_cast<double>(hfGainParam_->load()),
                               static_cast<int>(hfFreqParam_->load()));
    dsp_.snapToCurrentParams();

    setLatencySamples(dsp_.latencySamples());
}

void LangevinProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    if (numChannels < 1 || numSamples == 0) return;
    if (bypassParam_->load() >= 0.5f) return;

    // Read all parameter values upfront
    const bool   midSide     = processModeParam_->load() >= 0.5f;
    const double mainLFGain  = static_cast<double>(lfGainParam_->load());
    const int    mainLFFreq  = static_cast<int>(lfFreqParam_->load());
    const double mainHFGain  = static_cast<double>(hfGainParam_->load());
    const int    mainHFFreq  = static_cast<int>(hfFreqParam_->load());
    const double sideLFGain  = static_cast<double>(lfGainSParam_->load());
    const int    sideLFFreq  = static_cast<int>(lfFreqSParam_->load());
    const double sideHFGain  = static_cast<double>(hfGainSParam_->load());
    const int    sideHFFreq  = static_cast<int>(hfFreqSParam_->load());

    // Set process mode
    dsp_.setProcessMode(midSide ? Langevin::ProcessMode::MidSide
                                : Langevin::ProcessMode::Stereo);

    // Channel 0 always gets main params (Mid in M/S, or L/R linked in Stereo)
    dsp_.setChannelLFParams(0, mainLFGain, mainLFFreq);
    dsp_.setChannelHFParams(0, mainHFGain, mainHFFreq);

    if (numChannels >= 2)
    {
        // Stereo: channel 1 gets main params; M/S: channel 1 gets side params
        if (midSide)
        {
            dsp_.setChannelLFParams(1, sideLFGain, sideLFFreq);
            dsp_.setChannelHFParams(1, sideHFGain, sideHFFreq);
        }
        else
        {
            dsp_.setChannelLFParams(1, mainLFGain, mainLFFreq);
            dsp_.setChannelHFParams(1, mainHFGain, mainHFFreq);
        }

        const float* const inputs[2]  = { buffer.getReadPointer(0), buffer.getReadPointer(1) };
        float* const       outputs[2] = { buffer.getWritePointer(0), buffer.getWritePointer(1) };
        dsp_.process(inputs, outputs, 2, numSamples);
    }
    else
    {
        // Mono: process channel 0 only through channel strip 0
        float* channelData = buffer.getWritePointer(0);

        for (int i = 0; i < numSamples; ++i)
        {
            channelData[i] = static_cast<float>(
                dsp_.processMonoSample(static_cast<double>(channelData[i])));
        }
    }
}

juce::AudioProcessorEditor* LangevinProcessor::createEditor()
{
    return static_cast<juce::AudioProcessorEditor*>(new LangevinEditor(*this));
}

void LangevinProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void LangevinProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LangevinProcessor();
}
