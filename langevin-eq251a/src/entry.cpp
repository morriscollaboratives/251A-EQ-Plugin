// ==========================================================================
// entry.cpp — VST3 Plugin Factory for Langevin EQ-251A
// ==========================================================================

#include "processor.h"
#include "controller.h"
#include "plugids.h"
#include "version.h"

#include "public.sdk/source/main/pluginfactory.h"

using namespace Steinberg::Vst;
using namespace Langevin;

// --------------------------------------------------------------------------
// VST3 Plugin Entry
// --------------------------------------------------------------------------

BEGIN_FACTORY_DEF(stringCompanyName, stringCompanyWeb, stringCompanyEmail)

    // Audio Processor component
    DEF_CLASS2(
        INLINE_UID_FROM_FUID(ProcessorUID),
        PClassInfo::kManyInstances,
        kVstAudioEffectClass,
        stringPluginName,
        Vst::kDistributable,
        LANGEVIN_VST3_CATEGORY,
        FULL_VERSION_STR,
        kVstVersionString,
        LangevinProcessor::createInstance
    )

    // Edit Controller component
    DEF_CLASS2(
        INLINE_UID_FROM_FUID(ControllerUID),
        PClassInfo::kManyInstances,
        kVstComponentControllerClass,
        stringPluginName " Controller",
        0,
        "",
        FULL_VERSION_STR,
        kVstVersionString,
        LangevinController::createInstance
    )

END_FACTORY
