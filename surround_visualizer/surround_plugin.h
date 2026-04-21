/*
==============================================================================
BEGIN_JUCE_MODULE_DECLARATION
   ID:            surround_plugin
   vendor:        WolfSound
   version:       1.0.0
   name:          Surround Visualizer Plugin
   description:   Core of the surround sound visualizer plugin
   dependencies:  juce_audio_utils, juce_dsp
   website:       https://thewolfsound.com
   license:       MIT
END_JUCE_MODULE_DECLARATION
==============================================================================
*/
#pragma once
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <memory>
#include <functional>
#include <ranges>
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include "include/Surround_Visualizer/SampleFifo.h"
#include "include/Surround_Visualizer/SharedMemoryBridge.h"
#include "include/Surround_Visualizer/Parameters.h"
#include "include/Surround_Visualizer/JsonSerializer.h"
#include "include/Surround_Visualizer/PluginProcessor.h"
#include "include/Surround_Visualizer/SurroundVisualizerComponent.h"
#include "include/Surround_Visualizer/SpectrumVisualizerComponent.h"
#include "include/Surround_Visualizer/PluginEditor.h"
