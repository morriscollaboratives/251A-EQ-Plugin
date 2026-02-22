// ==========================================================================
// controller.h — VST3 Edit Controller for Langevin EQ-251A
// ==========================================================================

#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"

namespace Langevin {

class LangevinController : public Steinberg::Vst::EditController {
public:
    static Steinberg::FUnknown* createInstance(void*) {
        return (Steinberg::Vst::IEditController*)new LangevinController;
    }

    // EditController overrides
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream* state) SMTG_OVERRIDE;
};

} // namespace Langevin
