cmake_minimum_required(VERSION 3.25.0)

# -------------------------------------------------------
# Langevin EQ-251A — Top-level build
#
# Builds:
#   - VST3 plugin (all platforms)
#   - Audio Unit v2 (macOS with Xcode generator)
#
# Usage:
#   Windows:  cmake .. -A x64 && cmake --build . --config Release
#   Linux:    cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build .
#   macOS:    cmake .. -G Xcode && cmake --build . --config Release
# -------------------------------------------------------

# Disable unnecessary SDK examples
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)

# Enable VSTGUI for custom editor
set(SMTG_ENABLE_VSTGUI_SUPPORT ON CACHE BOOL "" FORCE)

# Disable post-build symlink creation (requires admin on Windows)
# and post-build validator run (fails if symlink fails).
# The build script handles installation separately.
set(SMTG_PLUGIN_TARGET_PATH "" CACHE STRING "" FORCE)
set(SMTG_RUN_VST_VALIDATOR OFF CACHE BOOL "" FORCE)

# Point to the VST3 SDK
set(VST3_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/vst3sdk" CACHE PATH "VST3 SDK root")

# If Apple AudioUnit SDK is present next to vst3sdk, point to it
# (The SDK auto-detects this, but we can also set it explicitly)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/AudioUnitSDK/AudioUnitSDK.xcodeproj")
    set(SMTG_AUDIOUNIT_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/AudioUnitSDK" CACHE PATH "AudioUnit SDK" FORCE)
endif()

# Include the SDK
add_subdirectory(${VST3_SDK_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/vst3sdk)

# Include our plugin
add_subdirectory(langevin-eq251a)
