cmake_minimum_required(VERSION 3.22)

# Link the MSVC runtime statically so users do not need a separate
# Visual C++ Redistributable installation.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(TearedTune VERSION 3.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(TearedTune
    COMPANY_NAME "TearedTune Labs"
    PLUGIN_MANUFACTURER_CODE TtLb
    PLUGIN_CODE TrT3
    PRODUCT_NAME "TearedTune V3"
    FORMATS VST3
    VST3_CATEGORIES Fx "Pitch Shift"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(TearedTune PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PitchCorrector.h
    Source/Parameters.h
)

target_include_directories(TearedTune PRIVATE Source)

target_compile_definitions(TearedTune PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(TearedTune
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# Optional DSP test executable, excluded from the regular plugin build.
add_executable(TearedDspTest EXCLUDE_FROM_ALL tests/dsp_test.cpp)
target_include_directories(TearedDspTest PRIVATE Source)
