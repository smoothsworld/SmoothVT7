cmake_minimum_required(VERSION 3.22)
project(SmoothVT7 VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

# JUCE: point JUCE_DIR at a local checkout, or let CMake download it.
set(JUCE_DIR "" CACHE PATH "Path to a JUCE checkout (optional)")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND FORMATS AU)
endif()
# AAX: add AAX here after pointing JUCE at the AAX SDK (juce_set_aax_sdk_path)

juce_add_plugin(SmoothVT7
    COMPANY_NAME "Smooth Life"
    COMPANY_WEBSITE ""
    BUNDLE_ID com.smoothlife.smoothvt7
    PLUGIN_MANUFACTURER_CODE Smlf
    PLUGIN_CODE Svt7
    PRODUCT_NAME "SmoothVT-7"
    DESCRIPTION "Vacuum Tube Stereo Compressor"
    FORMATS ${FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect)

juce_generate_juce_header(SmoothVT7)

juce_add_binary_data(SmoothVT7Assets SOURCES Assets/faceplate_bg.png)

target_sources(SmoothVT7 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(SmoothVT7 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(SmoothVT7
    PRIVATE
        SmoothVT7Assets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline DSP test (no JUCE): cmake --build build --target dsp_test && ./build/dsp_test
add_executable(dsp_test tests/dsp_test.cpp)
enable_testing()
add_test(NAME dsp_test COMMAND dsp_test)

# Offline test of the real processor + editor: cmake --build build --target plugin_test && ./build/plugin_test_artefacts/Release/plugin_test
juce_add_console_app(plugin_test PRODUCT_NAME "plugin_test")
juce_generate_juce_header(plugin_test)
target_sources(plugin_test PRIVATE tests/plugin_test.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(plugin_test PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="SmoothVT-7")
target_link_libraries(plugin_test PRIVATE SmoothVT7Assets juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
