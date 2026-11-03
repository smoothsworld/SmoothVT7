// SmoothVT-7 — Smooth Life
#pragma once

#include <JuceHeader.h>
#include "DSP.h"

namespace ids
{
    inline const juce::String thr[2]     { "thr1", "thr2" };
    inline const juce::String gain[2]    { "gain1", "gain2" };
    inline const juce::String attack[2]  { "att1", "att2" };
    inline const juce::String release[2] { "rel1", "rel2" };
    inline const juce::String tube[2]    { "tube1", "tube2" };
    inline const juce::String meter[2]   { "meter1", "meter2" };   // false = GR, true = VU
    inline const juce::String stereo     { "stereo" };
    inline const juce::String scHpf      { "schpf" };
    inline const juce::String mix        { "mix" };
    inline const juce::String lowCut     { "lowcut" };
    inline const juce::String highCut    { "highcut" };
    inline const juce::String filterPost { "filterpost" };          // false = PRE, true = POST
    inline const juce::String os         { "os" };
    inline const juce::String compIn     { "compin" };
    inline const juce::String output     { "output" };
}

class SmoothVT7AudioProcessor : public juce::AudioProcessor
{
public:
    SmoothVT7AudioProcessor();
    ~SmoothVT7AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;

    // Meter feeds for the editor (written on the audio thread, read on the UI thread)
    std::atomic<float> meterGrDb[2]  { 0.0f, 0.0f };
    std::atomic<float> meterOutDb[2] { -100.0f, -100.0f };

private:
    void updateFilters (double fs);
    void setOversamplingIndex (int idx);
    smooth::CoreSettings readSettings() const;

    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 4> oversamplers; // [0] unused (1x)
    int osIndex = -1;

    smooth::CompressorCore core;
    std::array<smooth::Biquad, 2> lowCut, highCut;
    int lastLowCut = -1, lastHighCut = -1;

    juce::dsp::DryWetMixer<float> mixer { 512 };
    juce::SmoothedValue<float> outGain;

    double baseRate = 48000.0;
    int maxBlock = 512;

    // cached raw parameter pointers (lock-free reads on the audio thread)
    std::atomic<float>* pThr[2] {}; std::atomic<float>* pGain[2] {};
    std::atomic<float>* pAtt[2] {}; std::atomic<float>* pRel[2] {};
    std::atomic<float>* pTube[2] {};
    std::atomic<float>* pStereo {}; std::atomic<float>* pScHpf {};
    std::atomic<float>* pMix {}; std::atomic<float>* pLowCut {}; std::atomic<float>* pHighCut {};
    std::atomic<float>* pFilterPost {}; std::atomic<float>* pOs {}; std::atomic<float>* pCompIn {};
    std::atomic<float>* pOutput {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmoothVT7AudioProcessor)
};
