cmake_minimum_required(VERSION 3.22)
project(openTunerEQ VERSION 1.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    add_compile_options(/utf-8)
endif()
option(OTE_BUILD_PLUGIN "Build JUCE plugin formats" ON)
option(OTE_BUILD_TESTS "Build tests" ON)
add_library(tuner_core STATIC Source/Core/Notes.cpp Source/Core/Filters.cpp Source/Core/Equalizer.cpp)
target_include_directories(tuner_core PUBLIC Source/Core)
set_target_properties(tuner_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(OTE_BUILD_TESTS)
    enable_testing()
    add_executable(core_tests Tests/CoreTests.cpp)
    target_link_libraries(core_tests PRIVATE tuner_core)
    add_test(NAME core COMMAND core_tests)
endif()
if(OTE_BUILD_PLUGIN)
    include(FetchContent)
    FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.9 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
    juce_add_binary_data(TunerAssets SOURCES assets/analog-cyber-faceplate.png assets/emblem.png)
    set_target_properties(TunerAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set(plugin_formats VST3 Standalone)
    if(APPLE)
        list(APPEND plugin_formats AU)
    endif()
    juce_add_plugin(openTunerEQ
        COMPANY_NAME "Alan Marcero" BUNDLE_ID "com.alanmarcero.opentunereq"
        PRODUCT_NAME "openTunerEQ" PLUGIN_MANUFACTURER_CODE Amrc PLUGIN_CODE OtEq
        FORMATS ${plugin_formats} VST3_CATEGORIES Fx EQ
        IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE COPY_PLUGIN_AFTER_BUILD FALSE)
    target_sources(openTunerEQ PRIVATE
        Source/Plugin/Parameters.cpp Source/Plugin/PluginProcessor.cpp
        Source/Plugin/Theme.cpp Source/Plugin/BandPanel.cpp
        Source/Plugin/ResponseGraph.cpp Source/Plugin/PluginEditor.cpp)
    target_include_directories(openTunerEQ PUBLIC Source/Plugin)
    target_compile_definitions(openTunerEQ PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(openTunerEQ PRIVATE tuner_core TunerAssets
        PUBLIC juce::juce_audio_utils juce::juce_recommended_config_flags)
    if(OTE_BUILD_TESTS)
        add_executable(plugin_tests Tests/PluginTests.cpp)
        target_link_libraries(plugin_tests PRIVATE openTunerEQ tuner_core TunerAssets)
        add_test(NAME plugin COMMAND plugin_tests)
    endif()
endif()
