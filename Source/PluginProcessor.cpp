// SmoothVT-7 — Smooth Life
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace smooth;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout SmoothVT7AudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto dB = [] (float v, int) { return String (v, 1) + " dB"; };
    const StringArray attacks  { "0.5 ms", "1 ms", "3 ms", "10 ms", "30 ms", "80 ms" };
    const StringArray releases { "50 ms", "100 ms", "200 ms", "400 ms", "800 ms", "Auto" };

    for (int i = 0; i < 2; ++i)
    {
        const String n = " " + String (i + 1);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::thr[i], 1 }, "Threshold" + n,
                        NormalisableRange<float> (-40.0f, 0.0f, 0.1f), -18.0f,
                        AudioParameterFloatAttributes().withStringFromValueFunction (dB)));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::gain[i], 1 }, "Gain" + n,
                        NormalisableRange<float> (0.0f, 20.0f, 0.1f), 4.0f,
                        AudioParameterFloatAttributes().withStringFromValueFunction (dB)));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::attack[i], 1 }, "Attack" + n, attacks, 2));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::release[i], 1 }, "Release" + n, releases, 3));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::tube[i], 1 }, "Tube" + n,
                        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f,
                        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                        {
                            if (v < 0.48f) return "Harder " + String (juce::roundToInt ((0.5f - v) * 200.0f)) + "%";
                            if (v > 0.52f) return "Softer " + String (juce::roundToInt ((v - 0.5f) * 200.0f)) + "%";
                            return String ("Centre");
                        })));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::meter[i], 1 }, "Meter" + n + " VU", false,
                        AudioParameterBoolAttributes().withStringFromValueFunction ([] (bool v, int) { return v ? "VU" : "GR"; })));
    }

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::stereo, 1 }, "Stereo",
                    StringArray { "Dual Mono", "Link", "Link HPF" }, 1));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::scHpf, 1 }, "SC HPF",
                    StringArray { "Off", "60 Hz", "90 Hz", "120 Hz", "150 Hz", "200 Hz" }, 2));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::mix, 1 }, "Mix",
                    NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f,
                    AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                        { return String (juce::roundToInt (v * 100.0f)) + "% wet"; })));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::lowCut, 1 }, "Low Cut",
                    StringArray { "Off", "20 Hz", "30 Hz", "40 Hz", "60 Hz", "80 Hz", "120 Hz" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::highCut, 1 }, "High Cut",
                    StringArray { "8 kHz", "10 kHz", "12 kHz", "14 kHz", "16 kHz", "18 kHz", "Off" }, 6));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::filterPost, 1 }, "Filter Post", false,
                    AudioParameterBoolAttributes().withStringFromValueFunction ([] (bool v, int) { return v ? "Post" : "Pre"; })));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::os, 1 }, "Oversampling",
                    StringArray { "1x", "2x", "4x", "8x" }, 1));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::compIn, 1 }, "Comp In", true,
                    AudioParameterBoolAttributes().withStringFromValueFunction ([] (bool v, int) { return v ? "In" : "Out"; })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::output, 1 }, "Output Level",
                    NormalisableRange<float> (-9.0f, 9.0f, 0.1f), 0.0f,
                    AudioParameterFloatAttributes().withStringFromValueFunction (dB)));
    return layout;
}

//==============================================================================
SmoothVT7AudioProcessor::SmoothVT7AudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SmoothVT7", createLayout())
{
    for (int i = 0; i < 2; ++i)
    {
        pThr[i]  = apvts.getRawParameterValue (ids::thr[i]);
        pGain[i] = apvts.getRawParameterValue (ids::gain[i]);
        pAtt[i]  = apvts.getRawParameterValue (ids::attack[i]);
        pRel[i]  = apvts.getRawParameterValue (ids::release[i]);
        pTube[i] = apvts.getRawParameterValue (ids::tube[i]);
    }
    pStereo     = apvts.getRawParameterValue (ids::stereo);
    pScHpf      = apvts.getRawParameterValue (ids::scHpf);
    pMix        = apvts.getRawParameterValue (ids::mix);
    pLowCut     = apvts.getRawParameterValue (ids::lowCut);
    pHighCut    = apvts.getRawParameterValue (ids::highCut);
    pFilterPost = apvts.getRawParameterValue (ids::filterPost);
    pOs         = apvts.getRawParameterValue (ids::os);
    pCompIn     = apvts.getRawParameterValue (ids::compIn);
    pOutput     = apvts.getRawParameterValue (ids::output);
}

bool SmoothVT7AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

//==============================================================================
void SmoothVT7AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    baseRate = sampleRate;
    maxBlock = samplesPerBlock;
    const auto numCh = (size_t) juce::jmax (1, getTotalNumOutputChannels());

    for (size_t i = 1; i < oversamplers.size(); ++i)
    {
        oversamplers[i] = std::make_unique<juce::dsp::Oversampling<float>> (
            numCh, i, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        oversamplers[i]->initProcessing ((size_t) samplesPerBlock);
    }

    mixer.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numCh });
    mixer.setMixingRule (juce::dsp::DryWetMixingRule::linear);

    outGain.reset (sampleRate, 0.03);
    outGain.setCurrentAndTargetValue (smooth::dbToGain (pOutput->load()));

    for (auto& f : lowCut) f.reset();
    for (auto& f : highCut) f.reset();
    lastLowCut = lastHighCut = -1;
    updateFilters (sampleRate);

    osIndex = -1;
    setOversamplingIndex ((int) pOs->load());
}

void SmoothVT7AudioProcessor::setOversamplingIndex (int idx)
{
    idx = juce::jlimit (0, 3, idx);
    if (idx == osIndex) return;
    osIndex = idx;

    float latency = 0.0f;
    if (idx > 0)
    {
        oversamplers[(size_t) idx]->reset();
        latency = oversamplers[(size_t) idx]->getLatencyInSamples();
    }
    mixer.setWetLatency (latency);
    setLatencySamples (juce::roundToInt (latency));

    core.prepare (baseRate * smooth::tables::osFactor[(size_t) idx]);
    core.update (readSettings());
}

void SmoothVT7AudioProcessor::updateFilters (double fs)
{
    const int lc = (int) pLowCut->load();
    const int hc = (int) pHighCut->load();
    if (lc != lastLowCut)
    {
        lastLowCut = lc;
        for (auto& f : lowCut)
            lc == 0 ? f.setBypass() : f.setHighPass (fs, smooth::tables::lowCutHz[(size_t) lc]);
    }
    if (hc != lastHighCut)
    {
        lastHighCut = hc;
        for (auto& f : highCut)
            hc == 6 ? f.setBypass() : f.setLowPass (fs, smooth::tables::highCutHz[(size_t) hc]);
    }
}

smooth::CoreSettings SmoothVT7AudioProcessor::readSettings() const
{
    smooth::CoreSettings s;
    for (int i = 0; i < 2; ++i)
    {
        s.ch[i].thresholdDb = pThr[i]->load();
        s.ch[i].makeupDb    = pGain[i]->load();
        s.ch[i].attackIdx   = (int) pAtt[i]->load();
        s.ch[i].releaseIdx  = (int) pRel[i]->load();
        s.ch[i].tube        = pTube[i]->load();
    }
    s.stereo   = (smooth::StereoMode) (int) pStereo->load();
    s.scHpfIdx = (int) pScHpf->load();
    s.compIn   = pCompIn->load() > 0.5f;
    return s;
}

//==============================================================================
void SmoothVT7AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
    if (numSamples == 0 || numCh == 0) return;

    setOversamplingIndex ((int) pOs->load());
    updateFilters (baseRate);
    core.update (readSettings());

    juce::dsp::AudioBlock<float> block (buffer);
    auto ioBlock = block.getSubsetChannelBlock (0, (size_t) numCh);

    mixer.setWetMixProportion (pMix->load());
    mixer.pushDrySamples (ioBlock);

    const bool post = pFilterPost->load() > 0.5f;
    auto runFilters = [&]
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                d[i] = highCut[(size_t) ch].process (lowCut[(size_t) ch].process (d[i]));
        }
    };
    if (! post) runFilters();

    // --- tube compressor at the oversampled rate
    float grMax[2] = { 0.0f, 0.0f };
    auto runCore = [&] (juce::dsp::AudioBlock<float>& b)
    {
        const auto n = (int) b.getNumSamples();
        float* ptr[2] = { b.getChannelPointer (0), numCh > 1 ? b.getChannelPointer (1) : nullptr };
        float frame[2];
        for (int i = 0; i < n; ++i)
        {
            for (int ch = 0; ch < numCh; ++ch) frame[ch] = ptr[ch][i];
            core.processFrame (frame, numCh);
            for (int ch = 0; ch < numCh; ++ch) ptr[ch][i] = frame[ch];
        }
        for (int ch = 0; ch < numCh; ++ch) grMax[ch] = core.getGainReductionDb (ch);
    };

    if (osIndex > 0)
    {
        auto& os = *oversamplers[(size_t) osIndex];
        auto up = os.processSamplesUp (ioBlock);
        runCore (up);
        os.processSamplesDown (ioBlock);
    }
    else
    {
        runCore (ioBlock);
    }

    if (post) runFilters();

    mixer.mixWetSamples (ioBlock);

    outGain.setTargetValue (smooth::dbToGain (pOutput->load()));
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = outGain.getNextValue();
        for (int ch = 0; ch < numCh; ++ch) buffer.getWritePointer (ch)[i] *= g;
    }

    // --- meters
    for (int ch = 0; ch < 2; ++ch)
    {
        const int src = juce::jmin (ch, numCh - 1);
        meterGrDb[ch].store (grMax[src]);
        const float rms = buffer.getRMSLevel (src, 0, numSamples);
        meterOutDb[ch].store (juce::Decibels::gainToDecibels (rms, -100.0f));
    }
}

//==============================================================================
void SmoothVT7AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SmoothVT7AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* SmoothVT7AudioProcessor::createEditor()
{
    return new SmoothVT7AudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SmoothVT7AudioProcessor();
}
