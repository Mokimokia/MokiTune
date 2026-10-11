cmake_minimum_required(VERSION 3.22)

project(MokiTune VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Point JUCE_DIR at a JUCE checkout, e.g. -DJUCE_DIR="$HOME/Developer/JUCE".
set(JUCE_DIR "" CACHE PATH "Path to the JUCE source tree")
if (JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_DIR}" JUCE)
else()
    find_package(JUCE CONFIG QUIET)
endif()

if (NOT TARGET juce::juce_audio_utils)
    message(FATAL_ERROR
        "JUCE was not found. Clone JUCE and configure with -DJUCE_DIR=/path/to/JUCE.\n"
        "Example: git clone https://github.com/juce-framework/JUCE.git ~/Developer/JUCE")
endif()

juce_add_plugin(MokiTune
    COMPANY_NAME "Moki Audio"
    BUNDLE_ID com.mokiaudio.mokitune
    PLUGIN_MANUFACTURER_CODE Moki
    PLUGIN_CODE MkTu
    PRODUCT_NAME "MokiTune"
    DESCRIPTION "Real-time vocal pitch correction"
    VERSION "0.1.0"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS VST3 AU
    VST3_CATEGORIES Fx
)

juce_generate_juce_header(MokiTune)

target_sources(MokiTune PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

target_compile_definitions(MokiTune PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)

target_link_libraries(MokiTune PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
