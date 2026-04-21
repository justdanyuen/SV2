#include "surround_plugin.h"

namespace surround_vis {
namespace {

auto& addParameterToProcessor(juce::AudioProcessor& processor, auto parameter) {
  auto& result = *parameter;
  processor.addParameter(parameter.release());
  return result;
}

juce::AudioParameterChoice& createVoiceGroupParameter(juce::AudioProcessor& processor) {
  constexpr auto versionHint = 1;
  return addParameterToProcessor(
      processor,
      std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{"voice.group", versionHint},
          "Voice group",
          juce::StringArray{"Soprano", "Mezzo", "Alto", "Tenor", "Baritone", "Bass"},
          0));
}

juce::AudioParameterBool& createEnabledParameter(juce::AudioProcessor& processor) {
  constexpr auto versionHint = 1;
  return addParameterToProcessor(
      processor,
      std::make_unique<juce::AudioParameterBool>(
          juce::ParameterID{"instance.enabled", versionHint}, "Enabled", true));
}

juce::AudioParameterChoice& createRoleParameter(juce::AudioProcessor& processor) {
  constexpr auto versionHint = 1;
  return addParameterToProcessor(
      processor,
      std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{"instance.role", versionHint},
          "Plugin role",
          juce::StringArray{"Satellite", "Master"},
          0));
}

juce::AudioParameterFloat& createAnalysisTrimParameter(juce::AudioProcessor& processor) {
  constexpr auto versionHint = 1;
  return addParameterToProcessor(
      processor,
      std::make_unique<juce::AudioParameterFloat>(
          juce::ParameterID{"analysis.trim", versionHint},
          "Analysis trim",
          juce::NormalisableRange<float>{-12.f, 12.f, 0.1f},
          0.f,
          juce::AudioParameterFloatAttributes{}.withLabel("dB")));
}

}  // namespace

Parameters::Parameters(juce::AudioProcessor& processor)
    : voiceGroup{createVoiceGroupParameter(processor)},
      enabled{createEnabledParameter(processor)},
      role{createRoleParameter(processor)},
      analysisTrim{createAnalysisTrimParameter(processor)} {}

}  // namespace surround_vis
