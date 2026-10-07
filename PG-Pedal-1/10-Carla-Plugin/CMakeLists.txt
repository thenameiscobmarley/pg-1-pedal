# ==============================================================================
#  PG-1 Pedal - the pg audio PG-1 pedal as a VST3 for Carla (and a standalone app).
#  It runs the SAME core as the Seed3 firmware (../06-Firmware-DaisySeed/pg1/core),
#  so the screen, knobs, touch gestures and DSP behave exactly like the real pedal.
# ==============================================================================
cmake_minimum_required(VERSION 3.22)
project(PG1Pedal VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(JUCE_PATH "$ENV{HOME}/JUCE" CACHE PATH "Path to the JUCE source tree")
set(HARDWAREKIT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/HardwareKit" CACHE PATH "HardwareKit checkout")
set(FW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../06-Firmware-DaisySeed")

add_subdirectory(${JUCE_PATH} JUCE EXCLUDE_FROM_ALL)
juce_add_module("${HARDWAREKIT_PATH}/modules/hardwarekit")
find_package(X11 REQUIRED)   # HardwareKit polls the pointer from the X server on Linux

juce_add_plugin(PG1Pedal
    COMPANY_NAME                "pg audio"
    PLUGIN_MANUFACTURER_CODE    Khra
    PLUGIN_CODE                 Pg1p
    FORMATS                     VST3 Standalone
    PRODUCT_NAME                "PG-1 Pedal"
    BUNDLE_ID                   "com.pgaudio.pg1pedal"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx Delay
    COPY_PLUGIN_AFTER_BUILD     TRUE)

juce_add_binary_data(PG1Assets SOURCES assets/face-print.png)

target_sources(PG1Pedal PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PedalView.cpp
    ${FW_DIR}/pg1/core/PgCore.cpp
    ${FW_DIR}/pg1/core/PgPages.cpp
    ${FW_DIR}/pg1/core/PgHealth.cpp
    ${FW_DIR}/pg1/core/PgSplash.cpp
    ${FW_DIR}/pg1/core/PgLearn.cpp
    ${FW_DIR}/pg1/core/PgState.cpp
    ${FW_DIR}/pg1/core/PgUi.cpp
    ${FW_DIR}/libDaisy/src/util/oled_fonts.c)

target_include_directories(PG1Pedal PRIVATE
    Source
    ${FW_DIR}/pg1/core
    ${FW_DIR}/libDaisy/src
    ${FW_DIR}/DaisySP/Source)

target_compile_definitions(PG1Pedal PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(PG1Pedal
    PRIVATE
        PG1Assets
        juce::juce_audio_utils
        juce::juce_opengl
        hardwarekit
        X11::X11
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
