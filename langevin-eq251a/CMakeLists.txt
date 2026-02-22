cmake_minimum_required(VERSION 3.25.0)

project(LangevinEQ251A
    VERSION 1.0.0
    DESCRIPTION "Langevin EQ-251A Program Equalizer"
)

set(langevin_sources
    src/processor.cpp
    src/processor.h
    src/controller.cpp
    src/controller.h
    src/entry.cpp
    src/plugids.h
    src/version.h
    src/dsp/LangevinDSP.h
)

set(target LangevinEQ251A)

# smtg_add_vst3plugin internally calls smtg_target_add_library_main,
# which adds platform entry points (linuxmain/dllmain/macmain).
# That function requires public_sdk_SOURCE_DIR to be set in the calling
# scope. When the SDK is included via add_subdirectory from a parent
# CMakeLists.txt, the variable lives in a sibling scope and is NOT
# inherited here. We set it explicitly from the CACHE variable.
if(NOT DEFINED public_sdk_SOURCE_DIR)
    set(public_sdk_SOURCE_DIR "${VST3_SDK_ROOT}/public.sdk")
endif()

smtg_add_vst3plugin(${target} ${langevin_sources})

target_compile_features(${target}
    PUBLIC
        cxx_std_17
)

target_include_directories(${target}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${target}
    PRIVATE
        sdk
)

if(SMTG_MAC)
    smtg_target_set_bundle(${target}
        BUNDLE_IDENTIFIER "com.morriscollaboratives.vst3.langevin-eq251a"
        COMPANY_NAME "Morris Collaboratives"
    )

    # Audio Unit v2 wrapper (requires Xcode generator + AudioUnit SDK)
    # The smtg_target_add_auv2 function is only available when using
    # the Xcode generator with SMTG_ENABLE_AUV2_BUILDS enabled.
    if(XCODE AND SMTG_ENABLE_AUV2_BUILDS)
        smtg_target_add_auv2(LangevinEQ251A-au
            BUNDLE_NAME          "Langevin EQ-251A"
            BUNDLE_IDENTIFIER    "com.morriscollaboratives.audiounit.langevin-eq251a"
            INFO_PLIST_TEMPLATE  "${CMAKE_CURRENT_SOURCE_DIR}/resource/au-info.plist"
            VST3_PLUGIN_TARGET   LangevinEQ251A
        )
    endif()
elseif(SMTG_WIN)
    # Windows .rc file would go here if needed
endif()
