// SmoothVT-7 — Smooth Life
#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

//==============================================================================
// Faceplate coordinates (1400 x 650 plate space). They match the printed scales
// in Assets/faceplate_bg.png, which is rendered at 2x from design/render.html.
namespace layout
{
    struct Pos { float x, y; };
    inline float mirror (float x) { return 1400.0f - x; }

    // channel 1 (channel 2 is mirrored around the centre line)
    constexpr Pos attack     { 410, 178 }, release   { 548, 178 };
    constexpr Pos threshold  { 410, 380 }, gain      { 548, 380 };
    constexpr Pos tube       { 268, 380 }, meterSw   { 138, 380 };
    constexpr float vuX = 96, vuY = 104, vuW = 236, vuH = 150;

    constexpr Pos stereo     { 700, 178 }, scHpf     { 700, 380 };

    constexpr float row = 548;
    constexpr Pos mix        { 170, row },  lowCut   { 330, row };
    constexpr Pos filterSw   { 470, row + 4 }, highCut { 610, row };
    constexpr Pos oversample { 790, row },  compSw   { 950, row + 4 };
    constexpr Pos output     { 1130, row };
}

static Colour panelShadow() { return Colour (0xff3a0204); }

//==============================================================================
SmoothLookAndFeel::SmoothLookAndFeel()
{
    setColour (BubbleComponent::backgroundColourId, Colour (0xff1a0a0a));
    setColour (BubbleComponent::outlineColourId, Colour (0xff000000));
    setColour (TooltipWindow::textColourId, Colour (0xfff4ece2));
}

Font SmoothLookAndFeel::getSliderPopupFont (Slider&)
{
    return Font (FontOptions (13.0f, Font::bold));
}

void SmoothLookAndFeel::drawBubble (Graphics& g, BubbleComponent&, const Point<float>&, const Rectangle<float>& body)
{
    g.setColour (Colour (0xf01a0a0a));
    g.fillRoundedRectangle (body, 4.0f);
    g.setColour (Colour (0x40ffffff));
    g.drawRoundedRectangle (body.reduced (0.5f), 4.0f, 1.0f);
}

static void softShadow (Graphics& g, Point<float> c, float r, float alpha)
{
    ColourGradient sg (panelShadow().withAlpha (alpha), c.x, c.y + 4.0f,
                       panelShadow().withAlpha (0.0f), c.x + r + 7.0f, c.y + 4.0f, true);
    g.setGradientFill (sg);
    g.fillEllipse (c.x - r - 7.0f, c.y + 4.0f - r - 7.0f, (r + 7.0f) * 2.0f, (r + 7.0f) * 2.0f);
}

void SmoothLookAndFeel::drawKnob (Graphics& g, Point<float> c, float r, float angle)
{
    softShadow (g, c, r, 0.55f);

    // skirt
    ColourGradient skirt (Colour (0xff3a3a3b), c.x, c.y - r, Colour (0xff050505), c.x, c.y + r, false);
    skirt.addColour (0.45, Colour (0xff151516));
    g.setGradientFill (skirt);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawEllipse (c.x - r + 0.5f, c.y - r + 0.5f, r * 2 - 1, r * 2 - 1, 1.0f);

    // satin top
    const float tr = r * 0.86f;
    ColourGradient top (Colour (0xff2b2b2c), c.x - tr * 0.1f, c.y - tr * 0.24f,
                        Colour (0xff111112), c.x + tr * 0.9f, c.y + tr * 0.9f, true);
    top.addColour (0.7, Colour (0xff1a1a1b));
    g.setGradientFill (top);
    g.fillEllipse (c.x - tr, c.y - tr - 0.5f, tr * 2, tr * 2);

    Path rim;
    rim.addCentredArc (c.x, c.y - 0.5f, tr, tr, 0.0f, -MathConstants<float>::halfPi * 0.95f, MathConstants<float>::halfPi * 0.95f, true);
    g.setColour (Colours::white.withAlpha (0.09f));
    g.strokePath (rim, PathStrokeType (1.2f));

    // pointer
    const Point<float> dir (std::sin (angle), -std::cos (angle));
    g.setColour (Colour (0xfff7f4ee));
    g.drawLine (Line<float> (c + dir * (r * 0.2f), c + dir * (r * 0.8f)), jmax (2.6f, r / 10.0f));
    g.fillEllipse (Rectangle<float> (jmax (2.6f, r / 10.0f), jmax (2.6f, r / 10.0f)).withCentre (c + dir * (r * 0.8f)));
}

void SmoothLookAndFeel::drawChickenHead (Graphics& g, Point<float> c, float r, float angle)
{
    softShadow (g, c, r * 0.95f, 0.5f);

    const Point<float> d (std::sin (angle), -std::cos (angle));   // pointing direction
    const Point<float> p (-d.y, d.x);                              // perpendicular
    const float L = r * 1.45f, w = r * 0.5f;

    const auto tip  = c + d * L;
    const auto b1   = c + p * w;
    const auto b2   = c - p * w;
    const auto back = c - d * (r * 0.75f);
    const auto m1   = c + d * (L * 0.55f) + p * (w * 0.42f);
    const auto m2   = c + d * (L * 0.55f) - p * (w * 0.42f);

    Path head;
    head.startNewSubPath (b1);
    head.quadraticTo (m1, tip);
    head.quadraticTo (m2, b2);
    head.quadraticTo (back - p * (w * 1.1f), back);
    head.quadraticTo (back + p * (w * 1.1f), b1);
    head.closeSubPath();

    ColourGradient skirt (Colour (0xff3a3a3b), c.x, c.y - L, Colour (0xff050505), c.x, c.y + L, false);
    skirt.addColour (0.45, Colour (0xff151516));
    g.setGradientFill (skirt);
    g.fillPath (head);
    g.setColour (Colours::black.withAlpha (0.8f));
    g.strokePath (head, PathStrokeType (1.0f));

    const float hr = r * 0.66f;
    ColourGradient top (Colour (0xff2b2b2c), c.x - hr * 0.1f, c.y - hr * 0.24f,
                        Colour (0xff111112), c.x + hr, c.y + hr, true);
    g.setGradientFill (top);
    g.fillEllipse (c.x - hr, c.y - hr, hr * 2, hr * 2);
    g.setColour (Colours::black.withAlpha (0.8f));
    g.drawEllipse (c.x - hr, c.y - hr, hr * 2, hr * 2, 1.0f);

    g.setColour (Colour (0xe6f7f4ee));
    g.drawLine (Line<float> (c + d * (r * 0.95f), c + d * (L * 0.88f)), 2.0f);
}

void SmoothLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                          float startAngle, float endAngle, Slider& s)
{
    const auto c = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float r = (float) s.getProperties().getWithDefault ("r", 30.0);
    const float angle = startAngle + pos * (endAngle - startAngle);
    if (s.getProperties()["style"].toString() == "chicken") drawChickenHead (g, c, r, angle);
    else                                                    drawKnob (g, c, r, angle);
}

//==============================================================================
void BatToggle::paintButton (Graphics& g, bool highlighted, bool)
{
    const auto c = getLocalBounds().toFloat().getCentre();

    // panel shadow
    ColourGradient sg (panelShadow().withAlpha (0.5f), c.x, c.y + 3, panelShadow().withAlpha (0.0f), c.x + 20, c.y + 3, true);
    g.setGradientFill (sg);
    g.fillEllipse (c.x - 20, c.y + 3 - 20, 40, 40);

    // chrome washer
    auto chrome = [&] (float rad)
    {
        ColourGradient cg (Colours::white, c.x - rad * 0.3f, c.y - rad * 0.35f, Colour (0xff4f504d), c.x + rad, c.y + rad, true);
        cg.addColour (0.45, Colour (0xffb6b7b4));
        return cg;
    };
    g.setGradientFill (chrome (14.0f));
    g.fillEllipse (c.x - 14, c.y - 14, 28, 28);
    g.setColour (Colour (0xff3a0a0a));
    g.drawEllipse (c.x - 14, c.y - 14, 28, 28, 0.8f);

    // hex nut
    Path hex;
    for (int i = 0; i < 6; ++i)
    {
        const float a = MathConstants<float>::pi / 3.0f * (float) i + MathConstants<float>::pi / 6.0f;
        const Point<float> pt (c.x + 9.0f * std::cos (a), c.y + 9.0f * std::sin (a));
        i == 0 ? hex.startNewSubPath (pt) : hex.lineTo (pt);
    }
    hex.closeSubPath();
    g.setGradientFill (chrome (9.0f));
    g.fillPath (hex);
    g.setColour (Colour (0xff555555));
    g.strokePath (hex, PathStrokeType (0.6f));
    g.setColour (Colour (0xff2a2a28));
    g.fillEllipse (c.x - 4.5f, c.y - 4.5f, 9, 9);

    // bat lever, thrown toward the active label (left = off, right = on)
    const float a = degreesToRadians (getToggleState() ? 32.0f : -32.0f);
    const Point<float> d (std::sin (a), -std::cos (a)), p (-d.y, d.x);
    const auto end = c + d * 25.0f;
    Path bat;
    bat.startNewSubPath (c + p * 3.0f);
    bat.lineTo (end + p * 4.5f);
    bat.lineTo (end - p * 4.5f);
    bat.lineTo (c - p * 3.0f);
    bat.closeSubPath();
    ColourGradient bg (Colour (0xff7d7e7b), (c - p * 5.0f).x, (c - p * 5.0f).y, Colour (0xff5f605d), (c + p * 5.0f).x, (c + p * 5.0f).y, false);
    bg.addColour (0.4, Colour (0xfffbfbf9));
    bg.addColour (0.6, Colour (0xffc9cac7));
    g.setGradientFill (bg);
    g.fillPath (bat);
    g.setColour (Colour (0xff444444));
    g.strokePath (bat, PathStrokeType (0.5f));
    g.setColour (Colour (0xfff2f2ef));
    g.fillEllipse (Rectangle<float> (10.0f, 10.0f).withCentre (end));
    g.setColour (Colour (0xff555555));
    g.drawEllipse (Rectangle<float> (10.0f, 10.0f).withCentre (end), 0.5f);

    if (highlighted)
    {
        g.setColour (Colours::white.withAlpha (0.06f));
        g.fillEllipse (c.x - 16, c.y - 16, 32, 32);
    }
}

//==============================================================================
float VuNeedle::dbToDegrees (float db)
{
    // printed scale positions (dB -> degrees from vertical)
    static const float dbs[]  { -26.0f, -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f, 4.0f };
    static const float degs[] { -52.0f, -46.0f, -33.0f, -24.0f, -16.0f, -6.0f, 5.0f, 13.0f, 22.0f, 31.0f, 42.0f, 47.0f };
    db = jlimit (dbs[0], dbs[11], db);
    for (int i = 0; i < 11; ++i)
        if (db <= dbs[i + 1])
            return jmap (db, dbs[i], dbs[i + 1], degs[i], degs[i + 1]);
    return degs[11];
}

void VuNeedle::setValueDb (float db, float dt)
{
    // VU-style ballistics: ~300 ms to settle
    const float k = 1.0f - std::exp (-dt / 0.075f);
    shownDb += (db - shownDb) * k;
    repaint();
}

void VuNeedle::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    Path clip; clip.addRoundedRectangle (b, 3.0f);
    g.reduceClipRegion (clip);

    const float cx = b.getCentreX(), cy = b.getBottom() + 6.0f;
    const float R = b.getHeight() * 0.8f + 4.0f;
    const float a = degreesToRadians (dbToDegrees (shownDb));

    g.setColour (Colours::black.withAlpha (0.18f));                     // needle shadow on the face
    g.drawLine (cx + 2.0f, cy, cx + 2.0f + R * std::sin (a), cy - R * std::cos (a) + 3.0f, 1.6f);
    g.setColour (Colour (0xff111111));
    g.drawLine (cx, cy, cx + R * std::sin (a), cy - R * std::cos (a), 1.4f);

    // pivot cover + zero-adjust screw
    g.setColour (Colour (0xff1b1b1b));
    g.fillRoundedRectangle (cx - 34.0f, b.getBottom() - 18.0f, 68.0f, 30.0f, 4.0f);
    const Point<float> sc (cx, b.getBottom() - 8.0f);
    ColourGradient sg (Colour (0xfff2f2f0), sc.x - 1.5f, sc.y - 1.5f, Colour (0xff4a4b49), sc.x + 4, sc.y + 4, true);
    g.setGradientFill (sg);
    g.fillEllipse (Rectangle<float> (8.0f, 8.0f).withCentre (sc));
    g.setColour (Colour (0xff333333));
    g.drawLine (sc.x - 3, sc.y, sc.x + 3, sc.y, 1.0f);

    // glass
    g.setColour (Colours::white.withAlpha (0.10f));
    g.fillRect (b.withHeight (b.getHeight() * 0.45f));
}

//==============================================================================
void SmoothVT7AudioProcessorEditor::Plate::paint (Graphics& g)
{
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (background, getLocalBounds().toFloat());
}

SmoothVT7AudioProcessorEditor::SmoothVT7AudioProcessorEditor (SmoothVT7AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    plate.background = ImageCache::getFromMemory (BinaryData::faceplate_bg_png, BinaryData::faceplate_bg_pngSize);
    plate.setBounds (0, 0, plateW, plateH);
    addAndMakeVisible (plate);

    using namespace layout;
    for (int ch = 0; ch < 2; ++ch)
    {
        auto X = [ch] (float x) { return ch == 0 ? x : mirror (x); };
        addRotary (ids::attack[ch],  Style::chicken, X (attack.x),    attack.y,    26, 270);
        addRotary (ids::release[ch], Style::chicken, X (release.x),   release.y,   26, 270);
        addRotary (ids::thr[ch],     Style::knob,    X (threshold.x), threshold.y, 34, 300);
        addRotary (ids::gain[ch],    Style::knob,    X (gain.x),      gain.y,      34, 300);
        addRotary (ids::tube[ch],    Style::chicken, X (tube.x),      tube.y,      19, 150);
        addToggle (ids::meter[ch], X (meterSw.x), meterSw.y);

        const float vx = ch == 0 ? vuX : 1400.0f - vuX - vuW;
        needles[ch].setBounds (roundToInt (vx), roundToInt (vuY), roundToInt (vuW), roundToInt (vuH));
        needles[ch].setInterceptsMouseClicks (false, false);
        plate.addAndMakeVisible (needles[ch]);
    }
    addRotary (ids::stereo, Style::chicken, stereo.x, stereo.y, 26, 120);
    addRotary (ids::scHpf,  Style::knob,    scHpf.x,  scHpf.y,  28, 300);

    addRotary (ids::mix,     Style::knob,    mix.x,        mix.y,        26, 300);
    addRotary (ids::lowCut,  Style::chicken, lowCut.x,     lowCut.y,     19, 270);
    addToggle (ids::filterPost, filterSw.x, filterSw.y);
    addRotary (ids::highCut, Style::chicken, highCut.x,    highCut.y,    19, 270);
    addRotary (ids::os,      Style::chicken, oversample.x, oversample.y, 19, 150);
    addToggle (ids::compIn, compSw.x, compSw.y);
    addRotary (ids::output,  Style::knob,    output.x,     output.y,     28, 300);

    setResizable (true, true);
    setResizeLimits (plateW / 2, plateH / 2, plateW * 3 / 2, plateH * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) plateW / plateH);
    setSize (plateW, plateH);

    lastTick = Time::getMillisecondCounter();
    startTimerHz (30);
}

SmoothVT7AudioProcessorEditor::~SmoothVT7AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SmoothVT7AudioProcessorEditor::addRotary (const String& paramId, Style style, float cx, float cy, float r, float sweepDeg)
{
    Control c;
    c.slider = std::make_unique<Slider> (Slider::RotaryHorizontalVerticalDrag, Slider::NoTextBox);
    auto& s = *c.slider;
    s.getProperties().set ("style", style == Style::chicken ? "chicken" : "knob");
    s.getProperties().set ("r", r);
    const float half = degreesToRadians (sweepDeg * 0.5f);
    s.setRotaryParameters (MathConstants<float>::twoPi - half, MathConstants<float>::twoPi + half, true);
    s.setMouseDragSensitivity (style == Style::chicken ? 160 : 260);
    s.setPopupDisplayEnabled (true, false, &plate, 900);
    s.setMouseCursor (MouseCursor::PointingHandCursor);

    const float extent = style == Style::chicken ? r * 1.55f : r + 4.0f;
    s.setBounds (Rectangle<float> (extent * 2, extent * 2).withCentre ({ cx, cy }).toNearestInt());
    plate.addAndMakeVisible (s);

    c.attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, s);
    if (auto* param = proc.apvts.getParameter (paramId))
    {
        s.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        s.setTooltip (param->getName (64));
    }
    rotaries.push_back (std::move (c));
}

void SmoothVT7AudioProcessorEditor::addToggle (const String& paramId, float cx, float cy)
{
    auto t = std::make_unique<BatToggle>();
    t->setBounds (Rectangle<float> (64.0f, 64.0f).withCentre ({ cx, cy }).toNearestInt());
    if (auto* param = proc.apvts.getParameter (paramId)) t->setTitle (param->getName (64));
    plate.addAndMakeVisible (*t);
    toggleAttachments.push_back (std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, paramId, *t));
    toggles.push_back (std::move (t));
}

void SmoothVT7AudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (Colour (0xff0b0b10));
}

void SmoothVT7AudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) plateW;
    plate.setTransform (AffineTransform::scale (scale));
}

void SmoothVT7AudioProcessorEditor::timerCallback()
{
    const auto now = Time::getMillisecondCounter();
    const float dt = jlimit (0.001f, 0.2f, (float) (now - lastTick) * 0.001f);
    lastTick = now;

    for (int ch = 0; ch < 2; ++ch)
    {
        const bool showVu = proc.apvts.getRawParameterValue (ids::meter[ch])->load() > 0.5f;
        // GR mode: needle rests at 0 VU and swings left with reduction.
        // VU mode: 0 VU = -18 dBFS RMS output.
        const float db = showVu ? proc.meterOutDb[ch].load() + 18.0f
                                : -proc.meterGrDb[ch].load();
        needles[ch].setValueDb (db, dt);
    }
}
