// ==========================================================================
// controller.h — VST3 Edit Controller for Langevin EQ-251A
// ==========================================================================

#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"

namespace Langevin {

class LangevinEditor;

class LangevinController : public Steinberg::Vst::EditController {
public:
    static Steinberg::FUnknown* createInstance(void*) {
        return (Steinberg::Vst::IEditController*)new LangevinController;
    }

    // EditController overrides
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream* state) SMTG_OVERRIDE;

    // Create custom GUI editor
    Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString name) SMTG_OVERRIDE;

    // Notify GUI when host changes parameters
    Steinberg::tresult PLUGIN_API setParamNormalized(
        Steinberg::Vst::ParamID tag,
        Steinberg::Vst::ParamValue value) SMTG_OVERRIDE;

private:
    LangevinEditor* guiEditor_ = nullptr;
};

} // namespace Langevin
