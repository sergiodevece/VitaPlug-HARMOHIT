cmake_minimum_required(VERSION 3.22)

project(VitaPlugHarmoHit VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep the dependency outside this product repository.  This project never
# writes into Gainer80; the path is only a local, pinned JUCE source reference.
set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../vitaplug_gainer_80/external/JUCE" CACHE PATH "Path to JUCE 9.0.3")
if (NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE 9.0.3 was not found. Set JUCE_DIR to a JUCE source checkout.")
endif()
add_subdirectory("${JUCE_DIR}" JUCE)

# Shared, embedded artwork.  Keeping the panel in BinaryData makes the editor
# self-contained in every AU/VST3 bundle and in the development preview host.
juce_add_binary_data(VitaPlugHarmoHitAssets
    SOURCES Resources/HARMOHIT/HarmoHitMixPanelArtworkRuntime.jpg)

if (WIN32)
    set(VITAPLUG_PLUGIN_FORMATS VST3)
else()
    set(VITAPLUG_PLUGIN_FORMATS AU VST3)
endif()

function(add_harmohit_plugin target product_name bundle_id plugin_code flavour_define)
    juce_add_plugin(${target}
        COMPANY_NAME "VITAPLUG"
        COMPANY_WEBSITE "https://vitaplug.audio"
        COMPANY_EMAIL "hello@vitaplug.audio"
        BUNDLE_ID "${bundle_id}"
        PLUGIN_MANUFACTURER_CODE Vtpl
        PLUGIN_CODE ${plugin_code}
        FORMATS ${VITAPLUG_PLUGIN_FORMATS}
        PRODUCT_NAME "${product_name}"
        PLUGIN_NAME "${product_name}"
        DESCRIPTION "Parallel harmonic enhancement and drive"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE)

    target_sources(${target} PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_include_directories(${target} PRIVATE Source)
    target_compile_definitions(${target} PRIVATE ${flavour_define}
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(${target} PRIVATE
        VitaPlugHarmoHitAssets
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endfunction()

add_harmohit_plugin(VitaPlugHarmoHitMix "VitaPlug HARMOHIT MIX"
    "com.vitaplug.harmohit.mix" HhMx VITAPLUG_HARMOHIT_MIX=1)
add_harmohit_plugin(VitaPlugHarmoHitMaster "VitaPlug HARMOHIT MASTER"
    "com.vitaplug.harmohit.master" HhMs VITAPLUG_HARMOHIT_MASTER=1)

enable_testing()
add_executable(VitaPlugHarmoHitDSPTests
    Tests/DSPTests.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)
target_include_directories(VitaPlugHarmoHitDSPTests PRIVATE Source)
target_compile_definitions(VitaPlugHarmoHitDSPTests PRIVATE VITAPLUG_HARMOHIT_MIX=1
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(VitaPlugHarmoHitDSPTests PRIVATE
    VitaPlugHarmoHitAssets
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
add_test(NAME VitaPlugHarmoHitDSPTests COMMAND VitaPlugHarmoHitDSPTests)

# Development-only visual host. It opens the real MIX editor in a JUCE window
# and writes a Component snapshot; it is not a plug-in format or installer.
juce_add_gui_app(VitaPlugHarmoHitMixEditorPreview
    PRODUCT_NAME "HARMOHIT MIX Editor Preview")
target_sources(VitaPlugHarmoHitMixEditorPreview PRIVATE
    Tests/EditorPreview.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)
target_include_directories(VitaPlugHarmoHitMixEditorPreview PRIVATE Source)
target_compile_definitions(VitaPlugHarmoHitMixEditorPreview PRIVATE
    VITAPLUG_HARMOHIT_MIX=1
    HARMOHIT_PROJECT_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(VitaPlugHarmoHitMixEditorPreview PRIVATE
    VitaPlugHarmoHitAssets
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
