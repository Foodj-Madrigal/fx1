cmake_minimum_required(VERSION 3.22)
project(MultiFX VERSION 1.0.0)

# Optionnel : chemin vers un JUCE deja telecharge. Sinon JUCE est telecharge automatiquement.
set(JUCE_LOCAL_DIR "" CACHE PATH "Chemin vers JUCE (optionnel)")
option(BUILD_DSP_TEST "Compiler le test du moteur audio" OFF)

if(JUCE_LOCAL_DIR)
    add_subdirectory(${JUCE_LOCAL_DIR} juce_build)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.6
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(MultiFX
    COMPANY_NAME "MonStudio"
    PLUGIN_MANUFACTURER_CODE Mnst
    PLUGIN_CODE Mfx1
    FORMATS VST3
    PRODUCT_NAME "Studio Multi FX"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(MultiFX PRIVATE
    Source/DSP.h
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp)

target_compile_definitions(MultiFX PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MultiFX
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

if(BUILD_DSP_TEST)
    juce_add_console_app(DspTest PRODUCT_NAME "DspTest")
    target_sources(DspTest PRIVATE test/DspTest.cpp)
    target_compile_definitions(DspTest PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    target_link_libraries(DspTest PRIVATE juce::juce_dsp juce::juce_recommended_config_flags)
endif()
