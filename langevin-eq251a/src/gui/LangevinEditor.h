// ==========================================================================
// LangevinEditor.h — VSTGUI Editor for Langevin EQ-251A
// Vintage skeuomorphic GUI with custom-drawn controls
// ==========================================================================

#pragma once

#include "public.sdk/source/vst/vstguieditor.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/cview.h"
#include "vstgui/lib/controls/ccontrol.h"
#include "../plugids.h"

namespace Langevin {

// ---- Layout constants ----
static constexpr int kEditorWidth  = 580;
static constexpr int kEditorHeight = 380;

// ---- Color palette (1960s industrial equipment) ----
namespace Colors {
    static const VSTGUI::CColor kFaceplate      {34, 34, 38, 255};     // dark gunmetal
    static const VSTGUI::CColor kFaceplateLight  {44, 44, 50, 255};     // subtle highlight
    static const VSTGUI::CColor kTextPrimary     {225, 215, 195, 255};  // warm cream (engraved)
    static const VSTGUI::CColor kTextSecondary   {130, 125, 115, 255};  // muted warm gray
    static const VSTGUI::CColor kTextDim         {80, 78, 72, 255};     // very dim
    static const VSTGUI::CColor kKnobBody        {28, 28, 32, 255};     // dark metal
    static const VSTGUI::CColor kKnobEdge        {62, 62, 68, 255};     // lighter ring
    static const VSTGUI::CColor kKnobHighlight   {48, 48, 54, 255};     // mid tone
    static const VSTGUI::CColor kPointer         {225, 215, 195, 255};  // cream pointer
    static const VSTGUI::CColor kAccentGold      {186, 142, 52, 255};   // gold branding
    static const VSTGUI::CColor kActiveAmber     {210, 165, 55, 255};   // amber LED
    static const VSTGUI::CColor kActiveGlow      {210, 165, 55, 80};    // amber glow
    static const VSTGUI::CColor kScrew           {50, 50, 56, 255};     // screw heads
    static const VSTGUI::CColor kScrewSlot       {30, 30, 34, 255};     // screw slot
    static const VSTGUI::CColor kBorder          {18, 18, 22, 255};     // dark border
    static const VSTGUI::CColor kDivider         {55, 55, 60, 255};     // section divider
}

// ---- Editor class ----
class LangevinEditor : public Steinberg::Vst::VSTGUIEditor,
                       public VSTGUI::IControlListener {
public:
    LangevinEditor(void* controller);
    ~LangevinEditor() override = default;

    // VSTGUIEditor / IPlugView
    bool PLUGIN_API open(void* parent,
                         const VSTGUI::PlatformType& platformType
                             = VSTGUI::PlatformType::kDefaultNative) override;
    void PLUGIN_API close() override;

    // IControlListener
    void valueChanged(VSTGUI::CControl* pControl) override;
    void controlBeginEdit(VSTGUI::CControl* pControl) override;
    void controlEndEdit(VSTGUI::CControl* pControl) override;

    // Called by controller when host changes a parameter
    void syncParameterValue(Steinberg::Vst::ParamID tag,
                            Steinberg::Vst::ParamValue value);

    DELEGATE_REFCOUNT(VSTGUIEditor)

private:
    // Control pointers (owned by frame, weak references)
    VSTGUI::CControl* lfGainKnob_    = nullptr;
    VSTGUI::CControl* lfFreqSwitch_  = nullptr;
    VSTGUI::CControl* hfGainKnob_    = nullptr;
    VSTGUI::CControl* hfFreqSwitch_  = nullptr;
    VSTGUI::CControl* bypassButton_  = nullptr;
};

} // namespace Langevin
