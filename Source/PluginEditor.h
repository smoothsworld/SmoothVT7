// SmoothVT-7 — Smooth Life
#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// Hardware-style drawing: satin black knobs, chicken-head selectors, chrome bat toggles
class SmoothLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SmoothLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>& tip,
                     const juce::Rectangle<float>& body) override;
    juce::Font getSliderPopupFont (juce::Slider&) override;

    static void drawKnob (juce::Graphics&, juce::Point<float> c, float r, float angle);
    static void drawChickenHead (juce::Graphics&, juce::Point<float> c, float r, float angle);
};

//==============================================================================
class BatToggle : public juce::ToggleButton
{
public:
    BatToggle() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

//==============================================================================
// VU meter needle + cap + glass, sitting on the meter face printed in the faceplate
class VuNeedle : public juce::Component
{
public:
    void setValueDb (float db, float dt);
    void paint (juce::Graphics&) override;
private:
    static float dbToDegrees (float db);
    float shownDb = 0.0f;
};

//==============================================================================
class SmoothVT7AudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SmoothVT7AudioProcessorEditor (SmoothVT7AudioProcessor&);
    ~SmoothVT7AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int plateW = 1400, plateH = 650;

private:
    void timerCallback() override;

    enum class Style { knob, chicken };
    struct Control
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    void addRotary (const juce::String& paramId, Style, float cx, float cy, float r, float sweepDeg);
    void addToggle (const juce::String& paramId, float cx, float cy);

    struct Plate : public juce::Component
    {
        juce::Image background;
        void paint (juce::Graphics&) override;
    };

    SmoothVT7AudioProcessor& proc;
    SmoothLookAndFeel lnf;
    Plate plate;
    std::vector<Control> rotaries;
    std::vector<std::unique_ptr<BatToggle>> toggles;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> toggleAttachments;
    VuNeedle needles[2];
    juce::uint32 lastTick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmoothVT7AudioProcessorEditor)
};
