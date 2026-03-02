#include "PluginEditor.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// =====================================================================
// Color palette — exact match to VSTGUI version
// =====================================================================
namespace Col {
    static const juce::Colour faceplate     { 34,  34,  38 };      // dark gunmetal
    static const juce::Colour faceplateLight { 44,  44,  50 };      // subtle highlight
    static const juce::Colour textPrimary   { 225, 215, 195 };     // warm cream (engraved)
    static const juce::Colour textSecondary { 130, 125, 115 };     // muted warm gray
    static const juce::Colour textDim       { 80,  78,  72 };      // very dim
    static const juce::Colour knobBody      { 28,  28,  32 };      // dark metal
    static const juce::Colour knobEdge      { 62,  62,  68 };      // lighter ring
    static const juce::Colour knobHighlight { 48,  48,  54 };      // mid tone
    static const juce::Colour pointer       { 225, 215, 195 };     // cream pointer
    static const juce::Colour accentGold    { 186, 142, 52 };      // gold branding
    static const juce::Colour activeAmber   { 210, 165, 55 };      // amber LED
    static const juce::Colour activeGlow    { 210, 165, 55 };      // amber glow (alpha applied in use)
    static const juce::Colour screw         { 50,  50,  56 };      // screw heads
    static const juce::Colour screwSlot     { 30,  30,  34 };      // screw slot
    static const juce::Colour border        { 18,  18,  22 };      // dark border
    static const juce::Colour divider       { 55,  55,  60 };      // section divider
}

// =====================================================================
// Helper: draw a screw head (matching VSTGUI version)
// =====================================================================
static void drawScrew(juce::Graphics& g, float cx, float cy, float r)
{
    // Shadow
    g.setColour(Col::border);
    g.fillEllipse(cx - r - 1.0f, cy - r + 1.0f, r * 2.0f, r * 2.0f);

    // Body
    g.setColour(Col::screw);
    g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);

    // Highlight crescent
    g.setColour(Col::knobEdge);
    g.fillEllipse(cx - r + 1.0f, cy - r + 1.0f, r * 2.0f - 3.0f, r * 2.0f - 3.0f);
    g.setColour(Col::screw);
    g.fillEllipse(cx - r + 2.0f, cy - r + 2.0f, r * 2.0f - 3.0f, r * 2.0f - 3.0f);

    // Slot
    g.setColour(Col::screwSlot);
    g.drawLine(cx - r * 0.5f, cy, cx + r * 0.5f, cy, 1.5f);
}

// =====================================================================
// Angle helpers — matching VSTGUI convention
// =====================================================================
static double knobValueToAngle(float val)
{
    // 270° sweep: -135° to +135° from 12 o'clock
    double degFrom12 = -135.0 + val * 270.0;
    return (-90.0 + degFrom12) * M_PI / 180.0;
}

static double freqSwitchAngle(float val, int numPos)
{
    double sweep = numPos <= 2 ? 90.0 : 180.0;
    double half = sweep / 2.0;
    double degFrom12 = -half + val * sweep;
    return (-90.0 + degFrom12) * M_PI / 180.0;
}

// =====================================================================
// VintageKnob — matching VSTGUI VintageKnob rendering
// =====================================================================
VintageKnob::VintageKnob(const juce::String& label) : label_(label)
{
    setSliderStyle(juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    setRange(-14.0, 14.0, 0.01);
}

void VintageKnob::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float outerR = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f - 30.0f;

    // --- Scale markings ---
    {
        const float marks[] = { -14.0f, -10.0f, -6.0f, -2.0f, 0.0f, 2.0f, 6.0f, 10.0f, 14.0f };
        for (int i = 0; i < 9; ++i)
        {
            float norm = (marks[i] + 14.0f) / 28.0f;
            double angle = knobValueToAngle(norm);
            float r1 = outerR + 5.0f;
            float r2 = outerR + 11.0f;

            g.setColour(marks[i] == 0.0f ? Col::textSecondary : Col::textDim);
            g.drawLine(cx + (float)std::cos(angle) * r1, cy + (float)std::sin(angle) * r1,
                       cx + (float)std::cos(angle) * r2, cy + (float)std::sin(angle) * r2,
                       marks[i] == 0.0f ? 1.5f : 0.8f);

            // Labels only at -14, 0, +14
            if (marks[i] == -14.0f || marks[i] == 0.0f || marks[i] == 14.0f)
            {
                float lr = outerR + 18.0f;
                float lx = cx + (float)std::cos(angle) * lr;
                float ly = cy + (float)std::sin(angle) * lr;
                char lbl[8];
                if (marks[i] == 0.0f)
                    snprintf(lbl, sizeof(lbl), "0");
                else
                    snprintf(lbl, sizeof(lbl), "%+.0f", marks[i]);

                g.setColour(Col::textSecondary);
                g.setFont(10.0f);
                g.drawText(lbl, juce::Rectangle<float>(lx - 22.0f, ly - 7.0f, 44.0f, 14.0f),
                           juce::Justification::centred, false);
            }
        }
    }

    // --- Knob shadow ---
    g.setColour(juce::Colour(10, 10, 14).withAlpha((uint8_t)180));
    g.fillEllipse(cx - outerR + 2.0f, cy - outerR + 3.0f, outerR * 2.0f, outerR * 2.0f);

    // --- Outer ring ---
    g.setColour(Col::knobEdge);
    g.fillEllipse(cx - outerR, cy - outerR, outerR * 2.0f, outerR * 2.0f);

    // --- Knob body ---
    const float bodyR = outerR - 3.0f;
    g.setColour(Col::knobBody);
    g.fillEllipse(cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f);

    // --- Highlight crescent (top-left) ---
    const float hlR = bodyR - 1.0f;
    g.setColour(Col::knobHighlight);
    g.fillEllipse(cx - hlR - 2.0f, cy - hlR - 2.0f, (hlR * 2.0f) - 2.0f, (hlR * 2.0f) - 2.0f);
    g.setColour(Col::knobBody);
    g.fillEllipse(cx - hlR + 1.0f, cy - hlR + 1.0f, (hlR - 1.0f) * 2.0f, (hlR - 1.0f) * 2.0f);

    // --- Grip lines (radial hash marks) ---
    g.setColour(juce::Colour(45, 45, 50));
    for (int i = 0; i < 36; ++i)
    {
        double a = i * 10.0 * M_PI / 180.0;
        float r1 = bodyR - 4.0f;
        float r2 = bodyR - 1.0f;
        g.drawLine(cx + r1 * (float)std::cos(a), cy + r1 * (float)std::sin(a),
                   cx + r2 * (float)std::cos(a), cy + r2 * (float)std::sin(a), 0.5f);
    }

    // --- Pointer line ---
    float norm = static_cast<float>((getValue() - getMinimum()) / (getMaximum() - getMinimum()));
    double angle = knobValueToAngle(norm);
    float pLen = bodyR - 8.0f;

    g.setColour(Col::pointer);
    g.drawLine(cx + 6.0f * (float)std::cos(angle), cy + 6.0f * (float)std::sin(angle),
               cx + pLen * (float)std::cos(angle), cy + pLen * (float)std::sin(angle), 2.5f);

    // --- Center cap ---
    g.setColour(Col::knobEdge);
    g.fillEllipse(cx - 5.0f, cy - 5.0f, 10.0f, 10.0f);

    // --- Value readout ---
    double v = getValue();
    char buf[16];
    if (std::abs(v) < 0.05)
        snprintf(buf, sizeof(buf), "0");
    else
        snprintf(buf, sizeof(buf), "%+.0f", v);

    g.setColour(Col::textPrimary);
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawText(buf, juce::Rectangle<float>(cx - 30.0f, cy + outerR + 6.0f, 60.0f, 16.0f),
               juce::Justification::centred, false);
}

// =====================================================================
// VintageFreqSwitch — matching VSTGUI VintageFreqSwitch rendering
// =====================================================================
VintageFreqSwitch::VintageFreqSwitch(const juce::StringArray& labels, int numPositions)
    : labels_(labels), numPos_(numPositions)
{
    setSliderStyle(juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    setRange(0.0, static_cast<double>(numPositions - 1), 1.0);
}

void VintageFreqSwitch::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float outerR = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f - 30.0f;

    const int currentPos = juce::roundToInt(getValue());

    // --- Position labels and detent dots ---
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    for (int i = 0; i < numPos_; ++i)
    {
        float posNorm = (float)i / std::max(1, numPos_ - 1);
        double angle = freqSwitchAngle(posNorm, numPos_);
        float lr = outerR + 22.0f;
        float lx = cx + (float)std::cos(angle) * lr;
        float ly = cy + (float)std::sin(angle) * lr;

        g.setColour(Col::textPrimary);
        g.drawText(labels_[i], juce::Rectangle<float>(lx - 26.0f, ly - 9.0f, 52.0f, 18.0f),
                   juce::Justification::centred, false);

        // Detent dot
        float dr = outerR + 7.0f;
        float dx = cx + (float)std::cos(angle) * dr;
        float dy = cy + (float)std::sin(angle) * dr;
        g.setColour(Col::textDim);
        g.fillEllipse(dx - 2.5f, dy - 2.5f, 5.0f, 5.0f);
    }

    // --- Switch body shadow ---
    g.setColour(juce::Colour(10, 10, 14).withAlpha((uint8_t)160));
    g.fillEllipse(cx - outerR + 1.0f, cy - outerR + 2.0f, outerR * 2.0f, outerR * 2.0f);

    // --- Outer ring ---
    g.setColour(Col::knobEdge);
    g.fillEllipse(cx - outerR, cy - outerR, outerR * 2.0f, outerR * 2.0f);

    // --- Body ---
    const float bodyR = outerR - 2.0f;
    g.setColour(Col::knobBody);
    g.fillEllipse(cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f);

    // --- Pointer ---
    float posNorm = (float)currentPos / std::max(1, numPos_ - 1);
    double angle = freqSwitchAngle(posNorm, numPos_);
    float pLen = bodyR - 4.0f;

    g.setColour(Col::pointer);
    g.drawLine(cx + 3.0f * (float)std::cos(angle), cy + 3.0f * (float)std::sin(angle),
               cx + pLen * (float)std::cos(angle), cy + pLen * (float)std::sin(angle), 2.0f);

    // --- Center cap ---
    g.setColour(Col::knobEdge);
    g.fillEllipse(cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);
}

// =====================================================================
// LangevinEditor
// =====================================================================
LangevinEditor::LangevinEditor(LangevinProcessor& p)
    : juce::AudioProcessorEditor(p), processor_(p)
{
    setSize(1120, 500);

    addAndMakeVisible(lfGainKnob_);
    addAndMakeVisible(lfFreqSwitch_);
    addAndMakeVisible(hfGainKnob_);
    addAndMakeVisible(hfFreqSwitch_);
    addAndMakeVisible(lfGainKnobS_);
    addAndMakeVisible(lfFreqSwitchS_);
    addAndMakeVisible(hfGainKnobS_);
    addAndMakeVisible(hfFreqSwitchS_);

    // --- Buttons ---
    auto setupBtn = [this](juce::TextButton& btn)
    {
        btn.setColour(juce::TextButton::buttonColourId, Col::knobBody);
        btn.setColour(juce::TextButton::textColourOffId, Col::textDim);
        addAndMakeVisible(btn);
    };
    setupBtn(stereoBtn_);
    setupBtn(bypassBtn_);
    setupBtn(msBtn_);

    stereoBtn_.onClick = [this]
    {
        processor_.apvts.getParameter("processMode")->setValueNotifyingHost(0.0f);
        updateModeButtons();
    };
    msBtn_.onClick = [this]
    {
        processor_.apvts.getParameter("processMode")->setValueNotifyingHost(1.0f);
        updateModeButtons();
    };
    bypassBtn_.onClick = [this]
    {
        auto* param = processor_.apvts.getParameter("bypass");
        float current = param->getValue();
        param->setValueNotifyingHost(current < 0.5f ? 1.0f : 0.0f);
        updateBypassButton();
    };

    // APVTS attachments
    lfGainAtt_  = std::make_unique<SliderAtt>(p.apvts, "lfGain",  lfGainKnob_);
    lfFreqAtt_  = std::make_unique<SliderAtt>(p.apvts, "lfFreq",  lfFreqSwitch_);
    hfGainAtt_  = std::make_unique<SliderAtt>(p.apvts, "hfGain",  hfGainKnob_);
    hfFreqAtt_  = std::make_unique<SliderAtt>(p.apvts, "hfFreq",  hfFreqSwitch_);
    lfGainSAtt_ = std::make_unique<SliderAtt>(p.apvts, "lfGainS", lfGainKnobS_);
    lfFreqSAtt_ = std::make_unique<SliderAtt>(p.apvts, "lfFreqS", lfFreqSwitchS_);
    hfGainSAtt_ = std::make_unique<SliderAtt>(p.apvts, "hfGainS", hfGainKnobS_);
    hfFreqSAtt_ = std::make_unique<SliderAtt>(p.apvts, "hfFreqS", hfFreqSwitchS_);

    updateModeButtons();
    updateBypassButton();
}

void LangevinEditor::updateModeButtons()
{
    const bool isMidSide = processor_.apvts.getRawParameterValue("processMode")->load() >= 0.5f;

    stereoBtn_.setColour(juce::TextButton::buttonColourId,
                         isMidSide ? Col::knobBody : juce::Colour(50, 50, 56));
    stereoBtn_.setColour(juce::TextButton::textColourOffId,
                         isMidSide ? Col::textDim : Col::accentGold);

    msBtn_.setColour(juce::TextButton::buttonColourId,
                     isMidSide ? juce::Colour(50, 50, 56) : Col::knobBody);
    msBtn_.setColour(juce::TextButton::textColourOffId,
                     isMidSide ? Col::accentGold : Col::textDim);

    stereoBtn_.repaint();
    msBtn_.repaint();
}

void LangevinEditor::updateBypassButton()
{
    const bool bypassed = processor_.apvts.getRawParameterValue("bypass")->load() >= 0.5f;

    bypassBtn_.setColour(juce::TextButton::buttonColourId,
                         bypassed ? juce::Colour(45, 35, 20) : Col::knobBody);
    bypassBtn_.setColour(juce::TextButton::textColourOffId,
                         bypassed ? Col::activeAmber : Col::textSecondary);
    bypassBtn_.repaint();
}

void LangevinEditor::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const float halfW = r.getWidth() / 2.0f;

    // Column centers (matching VSTGUI layout)
    const float midLx  = halfW * 0.25f;            // 140
    const float midRx  = halfW * 0.75f;            // 420
    const float sideLx = halfW + halfW * 0.25f;    // 700
    const float sideRx = halfW + halfW * 0.75f;    // 980
    const float midCx  = halfW / 2.0f;
    const float sideCx = halfW + halfW / 2.0f;

    // === Main background ===
    g.setColour(Col::faceplate);
    g.fillAll();

    // === Brushed-metal horizontal lines ===
    for (float y = r.getY() + 2.0f; y < r.getBottom(); y += 3.0f)
    {
        g.setColour(juce::Colour(
            (uint8_t)std::min(255, 34 + 2),
            (uint8_t)std::min(255, 34 + 2),
            (uint8_t)std::min(255, 38 + 2)));
        g.drawHorizontalLine(static_cast<int>(y), r.getX() + 4.0f, r.getRight() - 4.0f);
    }

    // === Outer border (recessed bevel) ===
    g.setColour(Col::border);
    g.drawRect(r, 2.0f);
    g.setColour(Col::knobHighlight);
    g.drawLine(r.getX() + 2.0f, r.getY() + 2.0f, r.getRight() - 2.0f, r.getY() + 2.0f, 0.5f);
    g.drawLine(r.getX() + 2.0f, r.getY() + 2.0f, r.getX() + 2.0f, r.getBottom() - 2.0f, 0.5f);

    // === Screws ===
    const float inset = 16.0f;
    drawScrew(g, r.getX() + inset, r.getY() + inset, 5.0f);
    drawScrew(g, r.getRight() - inset, r.getY() + inset, 5.0f);
    drawScrew(g, r.getX() + inset, r.getBottom() - inset, 5.0f);
    drawScrew(g, r.getRight() - inset, r.getBottom() - inset, 5.0f);

    // === Title block ===
    g.setColour(Col::accentGold);
    g.setFont(juce::Font(26.0f, juce::Font::bold));
    g.drawText("LANGEVIN",
               juce::Rectangle<float>(r.getX(), r.getY() + 14.0f, r.getWidth(), 30.0f),
               juce::Justification::centred);

    g.setColour(Col::textSecondary);
    g.setFont(13.0f);
    g.drawText("EQ-251A  PROGRAM EQUALIZER",
               juce::Rectangle<float>(r.getX(), r.getY() + 42.0f, r.getWidth(), 16.0f),
               juce::Justification::centred);

    // === Center divider ===
    g.setColour(Col::divider);
    g.drawLine(halfW, r.getY() + 62.0f, halfW, r.getBottom() - 40.0f, 1.0f);

    // === Section headers ===
    g.setColour(Col::textPrimary);
    g.setFont(juce::Font(16.0f, juce::Font::bold));
    g.drawText("M I D  /  S T E R E O",
               juce::Rectangle<float>(midCx - 120.0f, r.getY() + 64.0f, 240.0f, 20.0f),
               juce::Justification::centred);
    g.drawText("S I D E",
               juce::Rectangle<float>(sideCx - 100.0f, r.getY() + 64.0f, 200.0f, 20.0f),
               juce::Justification::centred);

    // === Column sub-headers ===
    g.setColour(Col::textSecondary);
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText("LOW FREQ",  juce::Rectangle<float>(midLx - 50.0f,  r.getY() + 86.0f, 100.0f, 14.0f), juce::Justification::centred);
    g.drawText("HIGH FREQ", juce::Rectangle<float>(midRx - 50.0f,  r.getY() + 86.0f, 100.0f, 14.0f), juce::Justification::centred);
    g.drawText("LOW FREQ",  juce::Rectangle<float>(sideLx - 50.0f, r.getY() + 86.0f, 100.0f, 14.0f), juce::Justification::centred);
    g.drawText("HIGH FREQ", juce::Rectangle<float>(sideRx - 50.0f, r.getY() + 86.0f, 100.0f, 14.0f), juce::Justification::centred);

    // === Knob sub-labels (GAIN dB / FREQUENCY) ===
    g.setFont(11.0f);
    g.setColour(Col::textSecondary);
    for (float x : { midLx, midRx, sideLx, sideRx })
    {
        g.drawText("GAIN  dB",
                   juce::Rectangle<float>(x - 50.0f, r.getY() + 286.0f, 100.0f, 14.0f),
                   juce::Justification::centred);
        g.drawText("FREQUENCY",
                   juce::Rectangle<float>(x - 50.0f, r.getY() + 412.0f, 100.0f, 14.0f),
                   juce::Justification::centred);
    }

    // === Column dividers within each half ===
    g.setColour(Col::textDim);
    g.drawLine(midCx, r.getY() + 96.0f, midCx, r.getBottom() - 40.0f, 0.3f);
    g.drawLine(sideCx, r.getY() + 96.0f, sideCx, r.getBottom() - 40.0f, 0.3f);

    // === Footer ===
    g.setColour(Col::textDim);
    g.setFont(10.0f);
    g.drawText("MORRIS  COLLABORATIVES",
               juce::Rectangle<float>(r.getX(), r.getBottom() - 26.0f, r.getWidth(), 18.0f),
               juce::Justification::centred);
}

void LangevinEditor::resized()
{
    // Matching VSTGUI layout exactly: halfW = 560
    // midLx=140, midRx=420, sideLx=700, sideRx=980
    const int knobW = 170, knobH = 170;
    const int swW = 140, swH = 120;
    const int knobY = 102;
    const int swY = 296;

    lfGainKnob_.setBounds(140 - knobW / 2, knobY, knobW, knobH);
    hfGainKnob_.setBounds(420 - knobW / 2, knobY, knobW, knobH);
    lfGainKnobS_.setBounds(700 - knobW / 2, knobY, knobW, knobH);
    hfGainKnobS_.setBounds(980 - knobW / 2, knobY, knobW, knobH);

    lfFreqSwitch_.setBounds(140 - swW / 2, swY, swW, swH);
    hfFreqSwitch_.setBounds(420 - swW / 2, swY, swW, swH);
    lfFreqSwitchS_.setBounds(700 - swW / 2, swY, swW, swH);
    hfFreqSwitchS_.setBounds(980 - swW / 2, swY, swW, swH);

    // Bottom bar buttons
    const int cx = getWidth() / 2;
    stereoBtn_.setBounds(cx - 130, 436, 88, 26);
    bypassBtn_.setBounds(cx - 38, 436, 76, 26);
    msBtn_.setBounds(cx + 42, 436, 88, 26);
}
