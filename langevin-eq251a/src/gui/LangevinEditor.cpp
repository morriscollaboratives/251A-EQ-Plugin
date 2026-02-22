// ==========================================================================
// LangevinEditor.cpp — VSTGUI Editor for Langevin EQ-251A
// Custom-drawn vintage skeuomorphic GUI
// ==========================================================================

#include "LangevinEditor.h"

#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/ccolor.h"
#include "vstgui/lib/cgraphicspath.h"

#include <cmath>
#include <string>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace VSTGUI;
using namespace Steinberg;
using namespace Steinberg::Vst;

namespace Langevin {

// =====================================================================
// Helper: draw a screw head
// =====================================================================
static void drawScrew(CDrawContext* ctx, CCoord cx, CCoord cy, CCoord r) {
    // Shadow
    ctx->setFillColor(Colors::kBorder);
    CRect shadow(cx - r - 1, cy - r + 1, cx + r - 1, cy + r + 1);
    ctx->drawEllipse(shadow, kDrawFilled);

    // Body
    ctx->setFillColor(Colors::kScrew);
    CRect body(cx - r, cy - r, cx + r, cy + r);
    ctx->drawEllipse(body, kDrawFilled);

    // Highlight (top-left crescent)
    ctx->setFillColor(Colors::kKnobEdge);
    CRect hl(cx - r + 1, cy - r + 1, cx + r - 2, cy + r - 2);
    ctx->drawEllipse(hl, kDrawFilled);
    ctx->setFillColor(Colors::kScrew);
    CRect inner(cx - r + 2, cy - r + 2, cx + r - 1, cy + r - 1);
    ctx->drawEllipse(inner, kDrawFilled);

    // Slot
    ctx->setFrameColor(Colors::kScrewSlot);
    ctx->setLineWidth(1.5);
    ctx->drawLine(CPoint(cx - r * 0.5, cy), CPoint(cx + r * 0.5, cy));
}

// =====================================================================
// FACEPLATE BACKGROUND VIEW
// =====================================================================
class VintageFaceplate : public CView {
public:
    VintageFaceplate(const CRect& size) : CView(size) {}

    void draw(CDrawContext* ctx) override {
        auto r = getViewSize();

        // --- Main background ---
        ctx->setFillColor(Colors::kFaceplate);
        ctx->drawRect(r, kDrawFilled);

        // --- Subtle brushed-metal horizontal lines ---
        for (CCoord y = r.top + 2; y < r.bottom; y += 3) {
            CColor line = Colors::kFaceplate;
            line.red   = (uint8_t)std::min(255, (int)line.red + 2);
            line.green = (uint8_t)std::min(255, (int)line.green + 2);
            line.blue  = (uint8_t)std::min(255, (int)line.blue + 2);
            ctx->setFrameColor(line);
            ctx->setLineWidth(0.5);
            ctx->drawLine(CPoint(r.left + 4, y), CPoint(r.right - 4, y));
        }

        // --- Outer border (recessed bevel) ---
        ctx->setFrameColor(Colors::kBorder);
        ctx->setLineWidth(2);
        ctx->drawRect(r, kDrawStroked);
        // Inner highlight on top & left
        ctx->setFrameColor(Colors::kKnobHighlight);
        ctx->setLineWidth(0.5);
        ctx->drawLine(CPoint(r.left + 2, r.top + 2),
                       CPoint(r.right - 2, r.top + 2));
        ctx->drawLine(CPoint(r.left + 2, r.top + 2),
                       CPoint(r.left + 2, r.bottom - 2));

        // --- Screws in corners ---
        CCoord screwR = 5;
        CCoord inset = 16;
        drawScrew(ctx, r.left + inset, r.top + inset, screwR);
        drawScrew(ctx, r.right - inset, r.top + inset, screwR);
        drawScrew(ctx, r.left + inset, r.bottom - inset, screwR);
        drawScrew(ctx, r.right - inset, r.bottom - inset, screwR);

        // --- Title block ---
        auto titleFont = makeOwned<CFontDesc>("Arial", 20, kBoldFace);
        ctx->setFont(titleFont);
        ctx->setFontColor(Colors::kAccentGold);
        CRect titleRect(r.left, r.top + 18, r.right, r.top + 44);
        ctx->drawString("LANGEVIN", titleRect, kCenterText);

        auto subFont = makeOwned<CFontDesc>("Arial", 10, kNormalFace);
        ctx->setFont(subFont);
        ctx->setFontColor(Colors::kTextSecondary);
        CRect subRect(r.left, r.top + 42, r.right, r.top + 56);
        ctx->drawString("EQ-251A  PROGRAM EQUALIZER", subRect, kCenterText);

        // --- Section divider (vertical center line) ---
        CCoord cx = r.left + r.getWidth() / 2;
        ctx->setFrameColor(Colors::kDivider);
        ctx->setLineWidth(0.5);
        ctx->drawLine(CPoint(cx, r.top + 68), CPoint(cx, r.bottom - 55));

        // --- Section labels ---
        auto sectionFont = makeOwned<CFontDesc>("Arial", 11, kBoldFace);
        ctx->setFont(sectionFont);
        ctx->setFontColor(Colors::kTextPrimary);

        CCoord lx = r.left + r.getWidth() * 0.25;
        CCoord rx = r.left + r.getWidth() * 0.75;

        CRect lfLabel(lx - 80, r.top + 62, lx + 80, r.top + 76);
        ctx->drawString("LOW  FREQUENCY", lfLabel, kCenterText);

        CRect hfLabel(rx - 80, r.top + 62, rx + 80, r.top + 76);
        ctx->drawString("HIGH  FREQUENCY", hfLabel, kCenterText);

        // --- Knob sub-labels ---
        auto labelFont = makeOwned<CFontDesc>("Arial", 9, kNormalFace);
        ctx->setFont(labelFont);
        ctx->setFontColor(Colors::kTextSecondary);

        CRect lfGainLabel(lx - 60, r.top + 202, lx + 60, r.top + 215);
        ctx->drawString("GAIN  dB", lfGainLabel, kCenterText);

        CRect hfGainLabel(rx - 60, r.top + 202, rx + 60, r.top + 215);
        ctx->drawString("GAIN  dB", hfGainLabel, kCenterText);

        CRect lfFreqLabel(lx - 60, r.top + 298, lx + 60, r.top + 311);
        ctx->drawString("FREQUENCY", lfFreqLabel, kCenterText);

        CRect hfFreqLabel(rx - 60, r.top + 298, rx + 60, r.top + 311);
        ctx->drawString("FREQUENCY", hfFreqLabel, kCenterText);

        // --- Footer ---
        auto footerFont = makeOwned<CFontDesc>("Arial", 8, kNormalFace);
        ctx->setFont(footerFont);
        ctx->setFontColor(Colors::kTextDim);
        CRect footer(r.left, r.bottom - 22, r.right, r.bottom - 8);
        ctx->drawString("MORRIS  COLLABORATIVES", footer, kCenterText);

        setDirty(false);
    }
};

// =====================================================================
// VINTAGE KNOB — large rotary gain control
// =====================================================================
class VintageKnob : public CControl {
public:
    VintageKnob(const CRect& size, IControlListener* listener,
                int32_t tag, float defaultValue = 0.5f,
                float minVal = -14.f, float maxVal = 14.f)
        : CControl(size, listener, tag)
        , minDisplay_(minVal), maxDisplay_(maxVal)
    {
        setMin(0.f);
        setMax(1.f);
        setDefaultValue(defaultValue);
        setValue(defaultValue);
    }

    void draw(CDrawContext* ctx) override {
        auto r = getViewSize();
        CCoord cx = r.left + r.getWidth() / 2;
        CCoord cy = r.top + r.getHeight() / 2 - 4; // offset up slightly for label space
        CCoord outerR = std::min(r.getWidth(), r.getHeight()) / 2 - 6;

        // --- Scale markings ---
        drawScale(ctx, cx, cy, outerR + 4);

        // --- Knob shadow ---
        ctx->setFillColor(CColor(10, 10, 14, 180));
        CRect shadowR(cx - outerR + 2, cy - outerR + 3,
                       cx + outerR + 2, cy + outerR + 3);
        ctx->drawEllipse(shadowR, kDrawFilled);

        // --- Outer ring ---
        ctx->setFillColor(Colors::kKnobEdge);
        CRect outerRect(cx - outerR, cy - outerR,
                         cx + outerR, cy + outerR);
        ctx->drawEllipse(outerRect, kDrawFilled);

        // --- Knob body (slightly smaller) ---
        CCoord bodyR = outerR - 3;
        ctx->setFillColor(Colors::kKnobBody);
        CRect bodyRect(cx - bodyR, cy - bodyR, cx + bodyR, cy + bodyR);
        ctx->drawEllipse(bodyRect, kDrawFilled);

        // --- Subtle highlight (top-left crescent) ---
        CCoord hlR = bodyR - 1;
        ctx->setFillColor(Colors::kKnobHighlight);
        CRect hlRect(cx - hlR - 2, cy - hlR - 2, cx + hlR - 4, cy + hlR - 4);
        ctx->drawEllipse(hlRect, kDrawFilled);
        ctx->setFillColor(Colors::kKnobBody);
        CRect reBody(cx - hlR + 1, cy - hlR + 1, cx + hlR - 1, cy + hlR - 1);
        ctx->drawEllipse(reBody, kDrawFilled);

        // --- Grip lines (radial hash marks around edge) ---
        ctx->setFrameColor(CColor(45, 45, 50, 255));
        ctx->setLineWidth(0.5);
        for (int i = 0; i < 36; ++i) {
            double a = i * 10.0 * M_PI / 180.0;
            double r1 = bodyR - 4;
            double r2 = bodyR - 1;
            ctx->drawLine(CPoint(cx + r1 * std::cos(a), cy + r1 * std::sin(a)),
                          CPoint(cx + r2 * std::cos(a), cy + r2 * std::sin(a)));
        }

        // --- Pointer line ---
        double angle = valueToAngle(getValueNormalized());
        double pLen = bodyR - 8;
        CPoint pStart(cx + 6 * std::cos(angle), cy + 6 * std::sin(angle));
        CPoint pEnd(cx + pLen * std::cos(angle), cy + pLen * std::sin(angle));
        ctx->setFrameColor(Colors::kPointer);
        ctx->setLineWidth(2.5);
        ctx->drawLine(pStart, pEnd);

        // --- Center cap ---
        ctx->setFillColor(Colors::kKnobEdge);
        CRect capRect(cx - 5, cy - 5, cx + 5, cy + 5);
        ctx->drawEllipse(capRect, kDrawFilled);

        // --- Value readout ---
        float displayVal = minDisplay_ + getValueNormalized() * (maxDisplay_ - minDisplay_);
        char buf[16];
        if (std::abs(displayVal) < 0.05f)
            snprintf(buf, sizeof(buf), "0");
        else
            snprintf(buf, sizeof(buf), "%+.0f", displayVal);

        auto valFont = makeOwned<CFontDesc>("Arial", 10, kBoldFace);
        ctx->setFont(valFont);
        ctx->setFontColor(Colors::kTextPrimary);
        CRect valRect(cx - 25, cy + outerR + 6, cx + 25, cy + outerR + 20);
        ctx->drawString(buf, valRect, kCenterText);

        setDirty(false);
    }

    CMouseEventResult onMouseDown(CPoint& where,
                                   const CButtonState& buttons) override {
        if (!buttons.isLeftButton()) return kMouseEventNotHandled;
        beginEdit();
        lastY_ = where.y;
        startVal_ = getValueNormalized();
        return kMouseEventHandled;
    }

    CMouseEventResult onMouseMoved(CPoint& where,
                                    const CButtonState& buttons) override {
        if (!buttons.isLeftButton()) return kMouseEventNotHandled;
        float sensitivity = buttons.getModifierState() & kShift ? 800.f : 200.f;
        float delta = (float)(lastY_ - where.y) / sensitivity;
        float newVal = std::max(0.f, std::min(1.f, startVal_ + delta));
        setValueNormalized(newVal);
        valueChanged();
        invalid();
        lastY_ = where.y;
        startVal_ = newVal;
        return kMouseEventHandled;
    }

    CMouseEventResult onMouseUp(CPoint& where,
                                 const CButtonState& buttons) override {
        endEdit();
        return kMouseEventHandled;
    }

    CLASS_METHODS_NOCOPY(VintageKnob, CControl)

private:
    double valueToAngle(float val) const {
        // 270° sweep: -135° to +135° from 12 o'clock
        double degFrom12 = -135.0 + val * 270.0;
        return (-90.0 + degFrom12) * M_PI / 180.0;
    }

    void drawScale(CDrawContext* ctx, CCoord cx, CCoord cy, CCoord radius) {
        auto tickFont = makeOwned<CFontDesc>("Arial", 7, kNormalFace);
        ctx->setFont(tickFont);

        // Tick marks at specific dB values
        float marks[] = {-14, -10, -6, -2, 0, 2, 6, 10, 14};
        int nMarks = 9;

        for (int i = 0; i < nMarks; ++i) {
            float norm = (marks[i] - minDisplay_) / (maxDisplay_ - minDisplay_);
            double angle = valueToAngle(norm);

            // Tick line
            double r1 = radius + 1;
            double r2 = radius + 6;
            ctx->setFrameColor(Colors::kTextDim);
            ctx->setLineWidth(marks[i] == 0 ? 1.5 : 0.8);
            if (marks[i] == 0)
                ctx->setFrameColor(Colors::kTextSecondary);
            ctx->drawLine(CPoint(cx + r1 * std::cos(angle), cy + r1 * std::sin(angle)),
                          CPoint(cx + r2 * std::cos(angle), cy + r2 * std::sin(angle)));

            // Label for key values
            if (marks[i] == -14 || marks[i] == 0 || marks[i] == 14) {
                double lr = radius + 14;
                CCoord lx = cx + lr * std::cos(angle);
                CCoord ly = cy + lr * std::sin(angle);
                char lbl[8];
                if (marks[i] == 0)
                    snprintf(lbl, sizeof(lbl), "0");
                else
                    snprintf(lbl, sizeof(lbl), "%+.0f", marks[i]);
                ctx->setFontColor(Colors::kTextSecondary);
                CRect lr2(lx - 14, ly - 6, lx + 14, ly + 6);
                ctx->drawString(lbl, lr2, kCenterText);
            }
        }
    }

    float minDisplay_, maxDisplay_;
    CCoord lastY_ = 0;
    float startVal_ = 0;
};

// =====================================================================
// VINTAGE FREQ SWITCH — stepped rotary selector
// =====================================================================
class VintageFreqSwitch : public CControl {
public:
    VintageFreqSwitch(const CRect& size, IControlListener* listener,
                      int32_t tag, int numPositions,
                      const char* const* labels)
        : CControl(size, listener, tag)
        , numPos_(numPositions), labels_(labels)
    {
        setMin(0.f);
        setMax(1.f);
        setValue(0.f);
    }

    void draw(CDrawContext* ctx) override {
        auto r = getViewSize();
        CCoord cx = r.left + r.getWidth() / 2;
        CCoord cy = r.top + r.getHeight() / 2 - 2;
        CCoord outerR = std::min(r.getWidth(), r.getHeight()) / 2 - 16;

        // --- Position labels around the switch ---
        auto lblFont = makeOwned<CFontDesc>("Arial", 9, kNormalFace);
        ctx->setFont(lblFont);

        for (int i = 0; i < numPos_; ++i) {
            float posNorm = (float)i / std::max(1, numPos_ - 1);
            double angle = valueToAngle(posNorm);
            double lr = outerR + 16;
            CCoord lx = cx + lr * std::cos(angle);
            CCoord ly = cy + lr * std::sin(angle);
            ctx->setFontColor(Colors::kTextPrimary);
            CRect labelR(lx - 18, ly - 7, lx + 18, ly + 7);
            ctx->drawString(labels_[i], labelR, kCenterText);

            // Detent dot
            double dr = outerR + 5;
            CCoord dx = cx + dr * std::cos(angle);
            CCoord dy = cy + dr * std::sin(angle);
            ctx->setFillColor(Colors::kTextDim);
            CRect dot(dx - 2, dy - 2, dx + 2, dy + 2);
            ctx->drawEllipse(dot, kDrawFilled);
        }

        // --- Switch body shadow ---
        ctx->setFillColor(CColor(10, 10, 14, 160));
        CRect shR(cx - outerR + 1, cy - outerR + 2,
                   cx + outerR + 1, cy + outerR + 2);
        ctx->drawEllipse(shR, kDrawFilled);

        // --- Outer ring ---
        ctx->setFillColor(Colors::kKnobEdge);
        CRect outerRect(cx - outerR, cy - outerR,
                         cx + outerR, cy + outerR);
        ctx->drawEllipse(outerRect, kDrawFilled);

        // --- Body ---
        CCoord bodyR = outerR - 2;
        ctx->setFillColor(Colors::kKnobBody);
        CRect bodyRect(cx - bodyR, cy - bodyR, cx + bodyR, cy + bodyR);
        ctx->drawEllipse(bodyRect, kDrawFilled);

        // --- Pointer ---
        int currentPos = valueToPosition();
        float posNorm = (float)currentPos / std::max(1, numPos_ - 1);
        double angle = valueToAngle(posNorm);
        double pLen = bodyR - 4;

        ctx->setFrameColor(Colors::kPointer);
        ctx->setLineWidth(2.0);
        ctx->drawLine(CPoint(cx + 3 * std::cos(angle), cy + 3 * std::sin(angle)),
                       CPoint(cx + pLen * std::cos(angle), cy + pLen * std::sin(angle)));

        // --- Center cap ---
        ctx->setFillColor(Colors::kKnobEdge);
        CRect cap(cx - 3, cy - 3, cx + 3, cy + 3);
        ctx->drawEllipse(cap, kDrawFilled);

        setDirty(false);
    }

    CMouseEventResult onMouseDown(CPoint& where,
                                   const CButtonState& buttons) override {
        if (!buttons.isLeftButton()) return kMouseEventNotHandled;
        beginEdit();
        // Cycle to next position
        int pos = valueToPosition();
        pos = (pos + 1) % numPos_;
        float newVal = (float)pos / std::max(1, numPos_ - 1);
        setValueNormalized(newVal);
        valueChanged();
        invalid();
        endEdit();
        return kMouseEventHandled;
    }

    CLASS_METHODS_NOCOPY(VintageFreqSwitch, CControl)

private:
    double valueToAngle(float val) const {
        double sweep = numPos_ <= 2 ? 90.0 : 180.0;
        double half = sweep / 2;
        double degFrom12 = -half + val * sweep;
        return (-90.0 + degFrom12) * M_PI / 180.0;
    }

    int valueToPosition() const {
        float v = getValueNormalized();
        int pos = (int)(v * numPos_);
        return std::max(0, std::min(numPos_ - 1, pos));
    }

    int numPos_;
    const char* const* labels_;
};

// =====================================================================
// VINTAGE BYPASS BUTTON — toggle with amber LED
// =====================================================================
class VintageBypass : public CControl {
public:
    VintageBypass(const CRect& size, IControlListener* listener, int32_t tag)
        : CControl(size, listener, tag)
    {
        setMin(0.f);
        setMax(1.f);
        setValue(0.f);
    }

    void draw(CDrawContext* ctx) override {
        auto r = getViewSize();
        CCoord cx = r.left + r.getWidth() / 2;
        CCoord cy = r.top + r.getHeight() / 2;
        bool active = getValue() >= 0.5f;

        // --- Button background ---
        CRect btnRect(cx - 32, cy - 10, cx + 32, cy + 10);
        ctx->setFillColor(active ? CColor(45, 35, 20, 255)
                                  : Colors::kKnobBody);
        ctx->drawRect(btnRect, kDrawFilled);
        ctx->setFrameColor(Colors::kKnobEdge);
        ctx->setLineWidth(1.0);
        ctx->drawRect(btnRect, kDrawStroked);

        // --- LED dot ---
        CCoord ledR = 4;
        CCoord ledX = cx - 22;
        if (active) {
            // Glow
            ctx->setFillColor(Colors::kActiveGlow);
            CRect glow(ledX - ledR - 3, cy - ledR - 3,
                        ledX + ledR + 3, cy + ledR + 3);
            ctx->drawEllipse(glow, kDrawFilled);
            // Bright center
            ctx->setFillColor(Colors::kActiveAmber);
        } else {
            ctx->setFillColor(Colors::kTextDim);
        }
        CRect led(ledX - ledR, cy - ledR, ledX + ledR, cy + ledR);
        ctx->drawEllipse(led, kDrawFilled);

        // --- Label ---
        auto font = makeOwned<CFontDesc>("Arial", 9, kBoldFace);
        ctx->setFont(font);
        ctx->setFontColor(active ? Colors::kActiveAmber : Colors::kTextSecondary);
        CRect lbl(cx - 8, cy - 7, cx + 32, cy + 7);
        ctx->drawString("BYPASS", lbl, kLeftText);

        setDirty(false);
    }

    CMouseEventResult onMouseDown(CPoint& where,
                                   const CButtonState& buttons) override {
        if (!buttons.isLeftButton()) return kMouseEventNotHandled;
        beginEdit();
        setValue(getValue() >= 0.5f ? 0.f : 1.f);
        valueChanged();
        invalid();
        endEdit();
        return kMouseEventHandled;
    }

    CLASS_METHODS_NOCOPY(VintageBypass, CControl)
};

// =====================================================================
// EDITOR IMPLEMENTATION
// =====================================================================

static ViewRect kDefaultSize(0, 0, kEditorWidth, kEditorHeight);

LangevinEditor::LangevinEditor(void* controller)
    : VSTGUIEditor(controller, &kDefaultSize)
{
}

bool PLUGIN_API LangevinEditor::open(void* parent, const PlatformType& platformType) {
    CRect frameSize(0, 0, kEditorWidth, kEditorHeight);
    frame = new CFrame(frameSize, this);
    frame->setBackgroundColor(Colors::kFaceplate);

    // --- Background faceplate ---
    auto* faceplate = new VintageFaceplate(frameSize);
    frame->addView(faceplate);

    // --- Layout positions ---
    CCoord lx = kEditorWidth * 0.25;  // left section center
    CCoord rx = kEditorWidth * 0.75;  // right section center

    // --- LF Gain Knob ---
    CCoord knobW = 120, knobH = 120;
    CRect lfGainRect(lx - knobW/2, 78, lx + knobW/2, 78 + knobH);
    lfGainKnob_ = new VintageKnob(lfGainRect, this, kLFGain, 0.5f, -14.f, 14.f);
    frame->addView(lfGainKnob_);

    // --- HF Gain Knob ---
    CRect hfGainRect(rx - knobW/2, 78, rx + knobW/2, 78 + knobH);
    hfGainKnob_ = new VintageKnob(hfGainRect, this, kHFGain, 0.5f, -14.f, 14.f);
    frame->addView(hfGainKnob_);

    // --- LF Freq Switch (2-position: 40 / 100) ---
    static const char* lfFreqLabels[] = {"40", "100"};
    CCoord swW = 80, swH = 80;
    CRect lfFreqRect(lx - swW/2, 218, lx + swW/2, 218 + swH);
    lfFreqSwitch_ = new VintageFreqSwitch(lfFreqRect, this, kLFFreq, 2, lfFreqLabels);
    frame->addView(lfFreqSwitch_);

    // --- HF Freq Switch (4-position: 3k / 5k / 10k / 15k) ---
    static const char* hfFreqLabels[] = {"3k", "5k", "10k", "15k"};
    CRect hfFreqRect(rx - swW/2, 218, rx + swW/2, 218 + swH);
    hfFreqSwitch_ = new VintageFreqSwitch(hfFreqRect, this, kHFFreq, 4, hfFreqLabels);
    frame->addView(hfFreqSwitch_);

    // --- Bypass Button ---
    CRect bypassRect(kEditorWidth / 2 - 36, 330, kEditorWidth / 2 + 36, 355);
    bypassButton_ = new VintageBypass(bypassRect, this, kBypass);
    frame->addView(bypassButton_);

    // --- Sync all parameter values from controller ---
    if (auto* ctrl = getController()) {
        auto* ec = static_cast<EditController*>(ctrl);
        auto sync = [&](CControl* control, ParamID pid) {
            if (control) {
                ParamValue v = ec->getParamNormalized(pid);
                control->setValueNormalized((float)v);
                control->invalid();
            }
        };
        sync(lfGainKnob_, kLFGain);
        sync(lfFreqSwitch_, kLFFreq);
        sync(hfGainKnob_, kHFGain);
        sync(hfFreqSwitch_, kHFFreq);
        sync(bypassButton_, kBypass);
    }

    frame->open(parent, platformType);
    return true;
}

void PLUGIN_API LangevinEditor::close() {
    lfGainKnob_ = nullptr;
    lfFreqSwitch_ = nullptr;
    hfGainKnob_ = nullptr;
    hfFreqSwitch_ = nullptr;
    bypassButton_ = nullptr;

    if (frame) {
        frame->close();
        frame = nullptr;
    }
}

// --- GUI → Host: user changed a control ---
void LangevinEditor::valueChanged(CControl* pControl) {
    if (!pControl) return;
    auto tag = pControl->getTag();
    auto value = pControl->getValueNormalized();

    if (auto* ctrl = getController()) {
        auto* ec = static_cast<EditController*>(ctrl);
        ec->setParamNormalized(tag, value);
        ec->performEdit(tag, value);
    }
}

void LangevinEditor::controlBeginEdit(CControl* pControl) {
    if (!pControl) return;
    if (auto* ctrl = getController()) {
        static_cast<EditController*>(ctrl)->beginEdit(pControl->getTag());
    }
}

void LangevinEditor::controlEndEdit(CControl* pControl) {
    if (!pControl) return;
    if (auto* ctrl = getController()) {
        static_cast<EditController*>(ctrl)->endEdit(pControl->getTag());
    }
}

// --- Host → GUI: parameter changed by automation/state load ---
void LangevinEditor::syncParameterValue(Steinberg::Vst::ParamID tag,
                                        Steinberg::Vst::ParamValue value) {
    if (!frame) return;

    CControl* control = nullptr;
    switch (tag) {
        case kLFGain: control = lfGainKnob_; break;
        case kLFFreq: control = lfFreqSwitch_; break;
        case kHFGain: control = hfGainKnob_; break;
        case kHFFreq: control = hfFreqSwitch_; break;
        case kBypass: control = bypassButton_; break;
        default: return;
    }

    if (control) {
        control->setValueNormalized((float)value);
        control->invalid();
    }
}

} // namespace Langevin
