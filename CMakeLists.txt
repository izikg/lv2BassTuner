# BassTuner - Bass Guitar Pitch Detector
# LV2 plugin for Ubuntu and Darkglass Anagram
# SPDX-License-Identifier: ISC

cmake_minimum_required(VERSION 3.22...3.31)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(BassTuner
    LANGUAGES C CXX
    VERSION 1.0.0
)

# ---------------------------------------------------------------------------
# Darkglass custom Anagram LV2 wrapper (replaces standard JUCE LV2 export)
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(juce-anagram-lv2
    GIT_REPOSITORY https://github.com/Darkglass-Electronics/juce-anagram-lv2.git
    GIT_TAG main
    GIT_SHALLOW TRUE
    UPDATE_DISCONNECTED TRUE
)
FetchContent_MakeAvailable(juce-anagram-lv2)

# ---------------------------------------------------------------------------
# JUCE 8 — patched for cross-compilation to ARM64 (required for Anagram)
# ---------------------------------------------------------------------------
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.9
    GIT_SHALLOW TRUE
    PATCH_COMMAND patch -p1 -i "${JUCE_ANAGRAM_LV2_DIR}/cross-compilation-patches/v8.0.9.patch"
    UPDATE_DISCONNECTED TRUE
)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# Plugin target
# ---------------------------------------------------------------------------
juce_add_plugin(BassTuner
    COMPANY_NAME "YourName"
    COMPANY_WEBSITE "https://yourwebsite.com"
    PRODUCT_NAME "BassTuner"
    BUNDLE_ID "com.yourname.basstuner"
    COPY_PLUGIN_AFTER_BUILD FALSE
    DESCRIPTION "Bass guitar pitch and note detector"
    FORMATS LV2
    IS_MIDI_EFFECT FALSE
    IS_SYNTH FALSE
    LV2_URI "https://yourwebsite.com/plugins/basstuner"
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
)

# ---------------------------------------------------------------------------
# Anagram-specific setup: category, block images, freeware flag
# ---------------------------------------------------------------------------
juce_anagram_lv2_setup(BassTuner
    CATEGORY "lv2:AnalyserPlugin"
    BLOCK_IMAGE_OFF "images/block-off.png"
    BLOCK_IMAGE_ON  "images/block-on.png"
    IS_FREEWARE TRUE
)

# ---------------------------------------------------------------------------
# Compile definitions — trim binary size, disable unused JUCE subsystems
# ---------------------------------------------------------------------------
target_compile_definitions(BassTuner
    PUBLIC
        JUCE_USE_FLAC=0
        JUCE_USE_OGGVORBIS=0
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
)

# ---------------------------------------------------------------------------
# Recommended JUCE flags
# ---------------------------------------------------------------------------
target_link_libraries(BassTuner
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

target_sources(BassTuner
    PRIVATE
        Source/PluginProcessor.cpp
)
