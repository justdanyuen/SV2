cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

set(CMAKE_POLICY_DEFAULT_CMP0141 NEW CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.11" CACHE STRING "" FORCE)
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "" FORCE)

project(SurroundVisualizer)

if(PROJECT_IS_TOP_LEVEL)
  if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 23)
  endif()
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
      "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>" CACHE STRING "" FORCE)
endif()

set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs)
include(cmake/cpm.cmake)

# -------------------------------------------------------------------------
# JUCE 8.0.12 — same version as the original tremolo project
# -------------------------------------------------------------------------
cpmaddpackage(
  NAME          JUCE
  GIT_TAG       8.0.12
  GITHUB_REPOSITORY juce-framework/JUCE
  SOURCE_DIR    ${LIB_DIR}/juce
)

include(cmake/CompilerWarnings.cmake)
include(cmake/Util.cmake)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set_directory_properties(PROPERTIES
    JUCE_VST3_COPY_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3"
    JUCE_AU_COPY_DIR   "$ENV{HOME}/Library/Audio/Plug-Ins/Components"
    JUCE_AAX_COPY_DIR  "/Library/Application Support/Avid/Audio/Plug-Ins"
  )
endif()

# -------------------------------------------------------------------------
# Plugin target
# -------------------------------------------------------------------------
set(SV2_PLUGIN_NAME "SV2")
set(SV2_FULL_NAME "Surround Vocal Spectrum Visualizer")

# Point JUCE to the bundled AAX SDK
juce_set_aax_sdk_path("${CMAKE_CURRENT_SOURCE_DIR}/libs/juce/modules/juce_audio_plugin_client/AAX/SDK")

juce_add_plugin(
  SV2Plugin
  PRODUCT_NAME      ${SV2_PLUGIN_NAME}
  VERSION           0.0.0
  COMPANY_NAME      "WolfSound"
  PLUGIN_MANUFACTURER_CODE Wfsd
  PLUGIN_CODE       Svis
  FORMATS
    Standalone
    AU
    VST3
    AAX
  IS_SYNTH          FALSE
  NEEDS_MIDI_INPUT  FALSE
  NEEDS_MIDI_OUTPUT FALSE
  VST3_AUTO_MANIFEST FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  VST3_CATEGORIES   "Fx|Analyzer"
)

# Re-sign the standalone with microphone entitlement after build
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  add_custom_command(
    TARGET SV2Plugin_Standalone POST_BUILD
    COMMAND codesign --force --deep --sign -
      --entitlements "${CMAKE_CURRENT_SOURCE_DIR}/surround_visualizer/entitlements.plist"
      "$<TARGET_BUNDLE_DIR:SV2Plugin_Standalone>"
    COMMENT "Re-signing standalone with microphone entitlement"
  )
endif()

target_compile_definitions(SV2Plugin
  PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    SV2_PLUGIN_NAME="${SV2_PLUGIN_NAME}"
)

# -------------------------------------------------------------------------
# JUCE module dependencies
# juce_dsp is new here — required for juce::dsp::FFT and WindowingFunction.
# juce_audio_utils provides AudioProcessorEditor base and GUI helpers.
# -------------------------------------------------------------------------
target_link_libraries(
  SV2Plugin
  PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_dsp           # FFT, windowing
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_core          # MemoryMappedFile, File, Thread
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# -------------------------------------------------------------------------
# Source files
# -------------------------------------------------------------------------
target_sources(SV2Plugin
  PRIVATE
    surround_visualizer/source/Parameters.cpp
    surround_visualizer/source/PluginProcessor.cpp
    surround_visualizer/source/PluginEditor.cpp
    surround_visualizer/source/JsonSerializer.cpp
    surround_visualizer/source/SurroundVisualizerComponent.cpp
    surround_visualizer/source/SpectrumVisualizerComponent.cpp
)

target_include_directories(SV2Plugin
  PRIVATE
    surround_visualizer
    surround_visualizer/include
)

# -------------------------------------------------------------------------
# Optional: tests and examples (mirrors original tremolo project structure)
# -------------------------------------------------------------------------
option(BUILD_EXAMPLES "Build example apps" OFF)
if(BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

option(BUILD_TESTS "Build automated tests" OFF)
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/..)
