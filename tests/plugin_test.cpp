// Runs the real SmoothVT-7 processor offline: every oversampling factor, mono + stereo,
// checks for NaN/denormal blow-ups, latency reporting and that compression happens.
#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
#include <cstdio>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    int fails = 0;
    auto check = [&] (bool ok, const juce::String& what) { std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); if (! ok) ++fails; };

    for (int os = 0; os < 4; ++os)
    {
        std::unique_ptr<juce::AudioProcessor> p (createPluginFilter());
        auto& proc = dynamic_cast<SmoothVT7AudioProcessor&> (*p);
        proc.apvts.getParameter (ids::os)->setValueNotifyingHost (os / 3.0f);
        proc.setPlayConfigDetails (2, 2, 48000.0, 512);
        proc.prepareToPlay (48000.0, 512);

        juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi;
        double inE = 0, outE = 0; bool finite = true; float peak = 0;
        for (int blk = 0; blk < 200; ++blk)
        {
            for (int i = 0; i < 512; ++i)
            {
                const float v = 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * 220.0f * (float) (blk * 512 + i) / 48000.0f);
                buf.setSample (0, i, v); buf.setSample (1, i, v * 0.8f);
                if (blk > 100) inE += v * v;
            }
            proc.processBlock (buf, midi);
            for (int i = 0; i < 512; ++i)
            {
                const float y = buf.getSample (0, i);
                if (! std::isfinite (y)) finite = false;
                peak = std::max (peak, std::abs (y));
                if (blk > 100) outE += y * y;
            }
        }
        const float gainDb = (float) (10.0 * std::log10 (outE / inE));
        std::printf ("  %dx: latency %d samples, GR %.1f dB, out/in %.1f dB, peak %.2f\n",
                     1 << os, proc.getLatencySamples(), proc.meterGrDb[0].load(), gainDb, peak);
        check (finite, juce::String (1 << os) + "x: output finite");
        check (proc.meterGrDb[0].load() > 3.0f, juce::String (1 << os) + "x: compressing a hot signal");
        check (os == 0 ? proc.getLatencySamples() == 0 : proc.getLatencySamples() > 0, juce::String (1 << os) + "x: latency reported");
    }

    // mono layout + state round trip
    {
        std::unique_ptr<juce::AudioProcessor> p (createPluginFilter());
        juce::AudioProcessor::BusesLayout mono; mono.inputBuses.add (juce::AudioChannelSet::mono()); mono.outputBuses.add (juce::AudioChannelSet::mono());
        check (p->setBusesLayout (mono), "mono layout accepted");
        p->prepareToPlay (44100.0, 256);
        juce::AudioBuffer<float> buf (1, 256); juce::MidiBuffer midi;
        for (int b = 0; b < 50; ++b) { for (int i = 0; i < 256; ++i) buf.setSample (0, i, 0.3f * std::sin ((float) (b * 256 + i) * 0.05f)); p->processBlock (buf, midi); }
        check (std::isfinite (buf.getSample (0, 100)), "mono processing finite");

        auto& proc = dynamic_cast<SmoothVT7AudioProcessor&> (*p);
        proc.apvts.getParameter (ids::thr[0])->setValueNotifyingHost (0.25f);
        juce::MemoryBlock mb; p->getStateInformation (mb);
        std::unique_ptr<juce::AudioProcessor> p2 (createPluginFilter());
        p2->setStateInformation (mb.getData(), (int) mb.getSize());
        auto& proc2 = dynamic_cast<SmoothVT7AudioProcessor&> (*p2);
        check (std::abs (proc2.apvts.getRawParameterValue (ids::thr[0])->load() - (-30.0f)) < 0.11f, "state save/restore");
    }

    // editor builds and lays out
    {
        std::unique_ptr<juce::AudioProcessor> p (createPluginFilter());
        p->prepareToPlay (48000.0, 512);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditor());
        check (ed != nullptr && ed->getWidth() == 1400 && ed->getHeight() == 650, "editor opens at 1400x650");
        ed->setSize (700, 325);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        check (img.isValid(), "editor renders when scaled to 50%");
        ed->setSize (1400, 650);
        auto full = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        juce::File out ("/home/claude/SmoothVT7/build/editor_snapshot.png"); out.deleteFile();
        juce::FileOutputStream fos (out); juce::PNGImageFormat().writeImageToStream (full, fos);
    }

    std::printf (fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
    return fails;
}
